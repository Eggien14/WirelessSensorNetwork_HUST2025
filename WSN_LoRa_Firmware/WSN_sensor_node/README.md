# WSN_sensor_node

## Overview

This project contains the firmware for the **Sensor Node** in the WSN25 Wireless Sensor Network. Each sensor node is a leaf node at the bottom of the network hierarchy. Its sole responsibility is to periodically measure temperature, air humidity, and soil moisture, then transmit the readings to its assigned Relay Node over LoRa radio.

The firmware runs on an **STM32F103C8T6** microcontroller and is developed with STM32CubeIDE using the STM32 HAL library.

---

## Hardware

| Component | Description |
|-----------|-------------|
| MCU | STM32F103C8T6 (Blue Pill) |
| Radio | SX1278 LoRa module, connected via SPI1 |
| Temperature / Humidity | DHT22 sensor on GPIO PA0 |
| Soil Moisture | Resistive/capacitive probe, read via ADC1 |
| Debug interface | UART2 (115200 baud), redirected to printf |

**SX1278 pin mapping (SPI1):**

| SX1278 Signal | STM32 Pin |
|---------------|-----------|
| CS (NSS) | PB0 |
| RESET | PB1 |
| DIO0 (RX done IRQ) | PA4 (EXTI4) |
| SCK / MISO / MOSI | SPI1 default pins |

---

## Directory Structure

```
WSN_sensor_node/
|-- Core/
|   |-- Inc/
|   |   |-- main.h          # Pin definitions, global includes
|   |   |-- lora_app.h      # Protocol constants, frame structures, function declarations
|   |   |-- sx1278_lora.h   # SX1278 driver interface
|   |   |-- gpio.h          # HAL GPIO init declarations
|   |   |-- spi.h           # HAL SPI1 init declarations
|   |   |-- usart.h         # HAL UART2 init declarations
|   |   |-- rtc.h           # HAL RTC init declarations
|   |   |-- tim.h           # HAL TIM1/TIM4 init declarations
|   |-- Src/
|   |   |-- main.c          # Application entry point and main loop
|   |   |-- lora_app.c      # LoRa application logic (registration + report phases)
|   |   |-- sx1278_lora.c   # SX1278 low-level driver
|   |   |-- gpio.c          # GPIO peripheral initialisation
|   |   |-- spi.c           # SPI1 peripheral initialisation
|   |   |-- usart.c         # UART2 peripheral initialisation
|   |   |-- rtc.c           # RTC peripheral initialisation
|   |   |-- tim.c           # TIM1/TIM4 peripheral initialisation
|   |   |-- stm32f1xx_hal_msp.c  # HAL MSP low-level init
|   |   |-- stm32f1xx_it.c       # Interrupt handlers
|   |   |-- syscalls.c      # Newlib syscall stubs
|   |   `-- sysmem.c        # Heap memory configuration
|   `-- Startup/
|       `-- startup_stm32f103c8tx.s  # ARM Cortex-M3 startup assembly
|-- Drivers/
|   |-- CMSIS/              # ARM CMSIS and STM32F1 device headers
|   `-- STM32F1xx_HAL_Driver/  # STM32 HAL source and headers
`-- WSN_sensor_node.ioc     # STM32CubeMX configuration file
```

---

## File Descriptions

### `Core/Inc/lora_app.h`
Central shared header for the entire protocol. Contains:

- **Node type selection:** `CURRENT_NODE_TYPE` macro determines which firmware variant is compiled. Set to `NODE_TYPE_SENSOR` for this project.
- **Node ID configuration:** `MY_SENSOR_ID` (e.g., `0xFA`) and `TARGET_RELAY_ID` (e.g., `0x03`) are hardcoded here before flashing.
- **Function codes:** One-byte identifiers for every message type in the protocol (see Protocol section below).
- **Timing constants:** All window durations (`REG_TIMEOUT_MS`, `SENSOR_TX_WINDOW_MS`, `SENSOR_MEASURE_WINDOW_MS`, `SENSOR_TDMA_BASE_MS`, `SENSOR_TDMA_SLOT_MS`, etc.).
- **Frame struct definitions:** Packed C structs for all message types shared across sensor, relay, and gateway firmware.

### `Core/Inc/sx1278_lora.h`
Defines the LoRa radio driver interface: operating modes (`SLEEP_MODE`, `STNBY_MODE`, `RXCONTIN_MODE`, `TRANSMIT_MODE`), bandwidth options, spreading factors, coding rates, power levels, and SX1278 register addresses.

### `Core/Src/main.c`
Application entry point. Performs hardware initialisation (GPIO, SPI1, TIM4, RTC, UART2, ADC1), initialises the SX1278 radio and DHT22/soil sensors, then:

1. Calls `LoRaApp_Sensor_RegistrationPhase()`  blocks until a TDMA slot is obtained from the relay.
2. Enters the main loop: executes the Report Phase on every wake cycle.

The main loop structure per cycle:
```
[Wake from STOP] -> Task 1: Send Data -> Task 2: Measure (every N cycles) -> Sleep (RTC alarm)
```

### `Core/Src/lora_app.c`
All LoRa application logic. Compiled with `CURRENT_NODE_TYPE == NODE_TYPE_SENSOR`. Contains:

- `RTC_SetAlarm_In_Seconds()`  programs the RTC counter alarm register directly to trigger a wake-up interrupt after a precise number of seconds.
- `Enter_Stop_Mode()`  suspends SysTick, enters STM32 STOP mode (low-power regulator on), resumes on RTC alarm interrupt.
- `LoRaApp_Sensor_RegistrationPhase()`  implements the Registration Phase (see Protocol section).
- `LoRaApp_Sensor_Task_SendData()`  implements the Report Phase transmission task with TDMA timing.
- `LoRaApp_Sensor_Task_Measure()`  reads all sensors and stores values in a static `msg_ss_data_t` buffer.
- `Pad_Execution_Time()`  busy-waits to fill the remainder of a fixed-duration task window, ensuring all nodes stay time-aligned.

### `Core/Src/sx1278_lora.c`
SX1278 hardware driver. Handles SPI register read/write, radio initialisation, mode switching, packet transmission, and packet reception via the DIO0 interrupt flag.

---

## Network Protocol

The Sensor Node participates in a two-phase duty-cycled protocol.

### Phase 1: Registration Phase (one-time at boot)

The goal is for the sensor to register with its target relay and receive a TDMA time slot and the current total cycle duration.

```
Sensor                              Relay
  |                                   |
  |-- ADV (0x01) ------------------>  |  [func | sensor_id | target_relay_id]
  |                                   |
  |  (wait up to REG_TIMEOUT_MS)      |
  |                                   |
  |  <-- ACK (0x02) ---------------   |  [func | relay_id | sensor_id | slot | total_cycle | reserved]
  |                                   |
  | (sleep 1 full cycle to sync)      |
  |                                   |
  |------ Enter Main Loop ----------  |
```

- The sensor broadcasts `FUNC_CODE_REG_ADV` (0x01) repeatedly until it receives a unicast reply `FUNC_CODE_REG_ACK` (0x02) addressed to its own ID from its target relay.
- The ACK contains: the **TDMA slot number** assigned to this sensor, and the **total cycle duration** (`TOTAL_CYCLE_SEC`) currently configured on the relay.
- After receiving the ACK, the sensor sleeps for exactly one full cycle (`TOTAL_CYCLE_SEC` seconds) using the RTC alarm, so that its wake-up time aligns with the start of the relay's receive window.

### Phase 2: Report Phase (repeated every cycle)

```
Cycle start (all nodes wake simultaneously)
  |
  [Task 1 - SENSOR_TX_WINDOW_MS = 3000 ms]
  |   Wait TDMA delay: SENSOR_TDMA_BASE_MS + (slot * SENSOR_TDMA_SLOT_MS)
  |   Transmit SS_DATA (0x03) x2
  |
  [Task 2 - SENSOR_MEASURE_WINDOW_MS = 3000 ms, every SENSOR_MEASURE_CYCLE cycles]
  |   Read DHT22 (temperature + air humidity)
  |   Read ADC (soil moisture)
  |   Store in sensor_latest_data buffer
  |
  [Sleep: TOTAL_CYCLE_SEC - active_time_ms/1000 seconds via RTC alarm]
```

### TDMA Collision Avoidance

Multiple sensors share the same radio channel and relay. Collisions are avoided by assigning each sensor a unique integer slot index during registration. Within the TX window, each sensor waits:

```
wait_ms = SENSOR_TDMA_BASE_MS + (slot * SENSOR_TDMA_SLOT_MS)
         = 1500 ms + (slot * 100 ms)
```

Slot 0 waits 1500 ms, slot 1 waits 1600 ms, slot 2 waits 1700 ms, and so on. The base delay of 1500 ms ensures the relay is fully awake before the first transmission.

### Power Management

The sensor spends most of its time in STM32 **STOP mode**, which reduces current consumption to approximately 20 µA (vs ~15 mA active). The STM32 LSE (32.768 kHz external crystal) continues to run the RTC in STOP mode. The RTC counter alarm register is loaded with the precise wake-up timestamp, triggering an EXTI line 17 interrupt that exits STOP mode.

---

## Configuration

Edit `Core/Inc/lora_app.h` before flashing:

| Macro | Default | Description |
|-------|---------|-------------|
| `CURRENT_NODE_TYPE` | `NODE_TYPE_SENSOR` | Selects sensor firmware variant |
| `MY_SENSOR_ID` | `0xFA` | Unique 1-byte ID of this node |
| `TARGET_RELAY_ID` | `0x03` | ID of the relay this sensor registers with |
| `DEFAULT_TOTAL_CYCLE` | `25` | Default cycle length in seconds (overridden by relay ACK) |
| `SENSOR_MEASURE_CYCLE` | `3` | Measure once every N report cycles |
| `SENSOR_TX_WINDOW_MS` | `3000` | Duration of the transmit task window |
| `SENSOR_MEASURE_WINDOW_MS` | `3000` | Duration of the measurement task window |
| `SENSOR_TDMA_BASE_MS` | `1500` | Base TDMA delay before first slot |
| `SENSOR_TDMA_SLOT_MS` | `100` | Time offset between consecutive TDMA slots |
| `REG_TIMEOUT_MS` | `2000` | Timeout waiting for registration ACK |

**LoRa radio settings** (in `main.c`, `initialize_lora()`):

| Parameter | Value |
|-----------|-------|
| Frequency | 433 MHz |
| Spreading Factor | SF7 |
| Bandwidth | 125 kHz |
| Coding Rate | CR 4/5 |
| TX Power | 20 dBm |
| Preamble | 8 symbols |

---

## Message Frame Reference

All frames share a common first byte: the **function code**.

**ADV  Sensor Registration Request (Sensor -> Relay)**
```
Byte 0: func_code  = 0x01
Byte 1: sensor_id
Byte 2: target_relay_id
Total: 3 bytes
```

**ACK  Registration Acknowledge (Relay -> Sensor)**
```
Byte 0: func_code  = 0x02
Byte 1: relay_id
Byte 2: target_sensor_id
Byte 3: time_slot (TDMA slot assigned)
Byte 4-5: total_cycle (uint16, seconds)
Byte 6: reserved
Total: 7 bytes
```

**SS_DATA  Sensor Data Report (Sensor -> Relay)**
```
Byte 0: func_code  = 0x03
Byte 1: sensor_id
Byte 2: target_relay_id
Byte 3-4: temp_val   (int16, value = actual_temp * 10)
Byte 5-6: hum_val    (uint16, value = actual_hum * 10)
Byte 7:   soil_val   (uint8, percentage 0-100)
Total: 8 bytes
```

---

## Build and Flash

1. Open the project in **STM32CubeIDE** (File -> Open Projects from File System).
2. Set the node ID and target relay ID in `Core/Inc/lora_app.h`.
3. Build: **Project -> Build All** (or Ctrl+B).
4. Flash: Connect ST-Link, then **Run -> Debug** or **Run -> Run**.

Debug output (printf over UART2 at 115200 baud) can be monitored with any serial terminal.
