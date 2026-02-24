# WSN_relay_node

## Overview

This project contains the firmware for the **Relay Node** in the WSN25 Wireless Sensor Network. The relay node sits in the middle tier of the network hierarchy. It manages a cluster of sensor nodes, collects their measurements, and aggregates them into a single uplink message to the gateway.

The relay firmware also participates in both network phases: it registers with the gateway to receive timing configuration, and it manages sensor registrations in the downlink direction. Each relay acts as a local cluster head, responsible for coordinating all sensors within its coverage area.

The firmware runs on an **STM32F103C8T6** microcontroller, developed with STM32CubeIDE using the STM32 HAL library.

---

## Hardware

| Component | Description |
|-----------|-------------|
| MCU | STM32F103C8T6 (Blue Pill) |
| Radio | SX1278 LoRa module, connected via SPI1 |
| Debug interface | UART2 (115200 baud), redirected to printf |

The relay node does not have any sensors attached. Its only peripheral beyond the SPI radio is the UART debug port. The pin mapping for SX1278 is identical to the sensor node.

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
WSN_relay_node/
|-- Core/
|   |-- Inc/
|   |   |-- main.h          # Pin definitions, global includes
|   |   |-- lora_app.h      # Protocol constants, frame structures, function declarations
|   |   |-- sx1278_lora.h   # SX1278 driver interface
|   |   |-- gpio.h          # HAL GPIO init declarations
|   |   |-- spi.h           # HAL SPI1 init declarations
|   |   |-- usart.h         # HAL UART2 init declarations
|   |   |-- rtc.h           # HAL RTC init declarations
|   |   `-- tim.h           # HAL TIM4 init declarations
|   |-- Src/
|   |   |-- main.c          # Application entry point and main loop
|   |   |-- lora_app.c      # LoRa application logic (registration + report phases)
|   |   |-- sx1278_lora.c   # SX1278 low-level driver
|   |   |-- gpio.c          # GPIO peripheral initialisation
|   |   |-- spi.c           # SPI1 peripheral initialisation
|   |   |-- usart.c         # UART2 peripheral initialisation
|   |   |-- rtc.c           # RTC peripheral initialisation
|   |   |-- tim.c           # TIM4 peripheral initialisation
|   |   |-- stm32f1xx_hal_msp.c  # HAL MSP low-level init
|   |   |-- stm32f1xx_it.c       # Interrupt handlers (DIO0 EXTI, RTC alarm)
|   |   |-- syscalls.c      # Newlib syscall stubs
|   |   `-- sysmem.c        # Heap memory configuration
|   `-- Startup/
|       `-- startup_stm32f103c8tx.s  # ARM Cortex-M3 startup assembly
|-- Drivers/
|   |-- CMSIS/              # ARM CMSIS and STM32F1 device headers
|   `-- STM32F1xx_HAL_Driver/  # STM32 HAL source and headers
`-- WSN_relay_node.ioc      # STM32CubeMX configuration file
```

---

## File Descriptions

### `Core/Inc/lora_app.h`
Shared protocol header (identical across all three STM32 firmware projects). For the relay node, `CURRENT_NODE_TYPE` is set to `NODE_TYPE_RELAY`. Key relay-specific items configured here:

- `MY_RELAY_ID`  unique 1-byte identifier for this relay (e.g., `0x03`).
- `MANAGED_SENSOR_LIST`  static list of sensor IDs this relay accepts (e.g., `{0xFA, 0xFE, 0xFD, 0xFC}`).
- `MANAGED_SENSOR_COUNT`  number of sensors in the managed list.
- `Relay_Reg_Queue_t`  struct tracking sensors that have sent a registration ADV and are awaiting an ACK.
- `Relay_Sensor_Data_Slot_t`  per-sensor data storage slot used to buffer readings within one cycle before forwarding.
- Relay timing constants: `RELAY_ACK_WINDOW_MS` (1000 ms), `RELAY_RX_WINDOW_MS` (8000 ms), `RELAY_GW_WINDOW_MS` (1000 ms).

### `Core/Src/main.c`
Application entry point. Performs hardware initialisation (GPIO, SPI1, TIM4, RTC, UART2), initialises the SX1278 radio, then:

1. Calls `LoRaApp_Relay_RegistrationWithGateway()`  blocks until the relay receives timing configuration from the gateway.
2. After synchronisation, enters the 3-task main loop per cycle.

**Main loop per cycle:**
```
[Wake from STOP]
  -> Reset data store        (LoRaApp_Relay_Init)
  -> Task 1: Send ACKs       (RELAY_ACK_WINDOW_MS = 1000 ms)
  -> Task 2: Listen sensors  (RELAY_RX_WINDOW_MS  = 8000 ms)
  -> Task 3: Forward to GW   (RELAY_GW_WINDOW_MS  = 1000 ms)
  -> Sleep via RTC alarm
```

### `Core/Src/lora_app.c`
All LoRa application logic, compiled with `CURRENT_NODE_TYPE == NODE_TYPE_RELAY`. Key functions:

- `LoRaApp_Relay_RegistrationWithGateway()`  Registration Phase with the gateway. Sends `RL_REG_ADV` (0x06) and blocks until it receives a broadcast `GW_REG_ACK` (0x07) containing its wakeup offset (`delta_t`). After receiving this, it sleeps for exactly `delta_t` seconds to align its cycle start time with the gateway's schedule.
- `LoRaApp_Relay_RxProcessing()`  Called in the Task 2 listen loop for every received packet. Dispatches on function code: `FUNC_CODE_REG_ADV` (0x01) queues the sensor for an ACK; `FUNC_CODE_SS_DATA` (0x03) saves the reading into the appropriate `Relay_Sensor_Data_Slot_t`.
- `LoRaApp_Relay_Task_SendACKs()`  Task 1. Iterates the ACK queue (`ackQueue`) built during the previous cycle's listen window. For each queued sensor, broadcasts a unicast `REG_ACK` (0x02) containing the sensor's TDMA slot (its index in `managed_sensors[]`) and the current `TOTAL_CYCLE_SEC`. Retransmits each ACK 3 times. Clears the queue after sending.
- `LoRaApp_Relay_Task_ForwardToGateway()`  Task 3. Assembles an `RL_DATA` (0x04) frame containing all readings collected in `relay_data_store[]` this cycle and transmits it to the gateway. Waits briefly for a `GW_ACK` (0x05) to confirm delivery.
- `IsSensorManaged()`  Checks if a received sensor ID belongs to this relay's `MANAGED_SENSOR_LIST`.
- `GetSensorIndex()`  Returns the array index of a sensor in `relay_data_store[]`, which also serves as the TDMA slot number.
- `LoRaApp_Relay_Init()`  Resets `has_data` flags and clears readings in `relay_data_store[]` at the start of each cycle, while preserving sensor IDs.

---

## Network Protocol

### Phase 1: Relay Registration with Gateway (one-time at boot)

Before the relay can operate, it must receive its timing configuration from the gateway. This includes the global cycle duration (`TOTAL_CYCLE_SEC`) and its individual **wakeup offset** (`delta_t`), which determines when within the global cycle this relay should become active.

```
Relay                              Gateway
  |                                    |
  |-- RL_REG_ADV (0x06) ----------->   |  [func | relay_id | reserved]
  |                                    |
  |  (wait up to REG_TIMEOUT_MS)       |
  |                                    |
  | <-- GW_REG_ACK (0x07) broadcast -- |  [func | cycle_H | cycle_L | count | id1 | dt_H1 | dt_L1 | ...]
  |                                    |
  | (scan broadcast for own ID)        |
  | (extract TOTAL_CYCLE + delta_t)    |
  |                                    |
  | (sleep delta_t seconds to sync)    |
  |                                    |
  |------- Enter Main Loop ---------   |
```

The gateway broadcasts the `GW_REG_ACK` frame containing configuration for all registered relays in a single packet. Each relay scans the list for its own ID to extract its assigned `delta_t`. The staggered wakeup offsets prevent all relays from transmitting to the gateway simultaneously at the end of their cycles.

### Phase 2: One Complete Relay Cycle

```
Cycle start (relay wakes, sensors also wake simultaneously)
 |
 [Task 1 - RELAY_ACK_WINDOW_MS = 1000 ms]
 |  For each sensor in ackQueue (from previous cycle):
 |    Broadcast REG_ACK (0x02) x3: [func | relay_id | sensor_id | tdma_slot | total_cycle]
 |  Clear ackQueue
 |
 [Task 2 - RELAY_RX_WINDOW_MS = 8000 ms]
 |  Continuous RX mode. For each received packet:
 |    If func = 0x01 (ADV):  add sensor_id to ackQueue (deduplicated)
 |    If func = 0x03 (DATA): save readings to relay_data_store[sensor_index]
 |                           (ignore if has_data already set for this cycle)
 |
 [Task 3 - RELAY_GW_WINDOW_MS = 1000 ms]
 |  Assemble RL_DATA (0x04) frame from relay_data_store[]
 |  Transmit to gateway
 |  Wait for GW_ACK (0x05)
 |
 [Sleep: TOTAL_CYCLE_SEC - 10 seconds via RTC alarm]
```

### Dual-Role Reception

During Task 2, the relay simultaneously handles two types of traffic interleaved on the same channel:

- **Registration ADV** (0x01) from sensors that are booting for the first time or waking after a reset. These are buffered in the `ackQueue` and served during Task 1 of the **next** cycle.
- **Data frames** (0x03) from sensors actively reporting. These are saved immediately to the data store for forwarding at the end of the current cycle.

The relay checks `target_relay_id` in every incoming frame and silently discards any packet not addressed to itself, since all radios share the same broadcast channel.

### TDMA Slot Assignment for Sensors

When the relay sends a `REG_ACK` to a sensor, it assigns a TDMA slot number equal to the sensor's index in `MANAGED_SENSOR_LIST`. This is determined by `GetSensorIndex()`. The relay's list is fixed at compile time, so slot assignments are deterministic:

```
MANAGED_SENSOR_LIST = {0xFA, 0xFE, 0xFD, 0xFC}
  Sensor 0xFA -> slot 0  (waits 1500 ms)
  Sensor 0xFE -> slot 1  (waits 1600 ms)
  Sensor 0xFD -> slot 2  (waits 1700 ms)
  Sensor 0xFC -> slot 3  (waits 1800 ms)
```

### Wakeup Offset and Inter-Relay Scheduling

The gateway assigns a different `delta_t` to each relay. After registration, each relay sleeps for exactly `delta_t` seconds to shift its active window forward in time. This means relay cycles are staggered across the global cycle, preventing relay-to-gateway collisions at the end of each cycle:

```
Global cycle timeline:
  t=0               t=delta_t_R1      t=delta_t_R2      t=T
  |--- Relay R1 active ---|                |               |
  |                       |--- Relay R2 active ---|        |
```

### RL_DATA Frame Format (Relay -> Gateway)

```
Byte 0:     func_code = 0x04
Byte 1:     relay_id
Byte 2:     sensor_count (number of sensor entries following)
For each sensor (6 bytes):
  Byte n+0: sensor_id
  Byte n+1: temp_H   (high byte of int16, value = temp * 10)
  Byte n+2: temp_L   (low byte)
  Byte n+3: hum_H    (high byte of uint16, value = hum * 10)
  Byte n+4: hum_L    (low byte)
  Byte n+5: soil     (uint8, percentage 0-100)
```

---

## Power Management

The relay uses the same RTC-based STOP mode mechanism as the sensor node. After completing all three tasks, the relay calculates the remaining sleep duration:

```
sleep_sec = TOTAL_CYCLE_SEC - (RELAY_ACK_WINDOW_MS + RELAY_RX_WINDOW_MS + RELAY_GW_WINDOW_MS) / 1000
          = TOTAL_CYCLE_SEC - 10 seconds
```

It then programs the RTC alarm and enters STOP mode. The STM32 LSE (32.768 kHz crystal) continues running the RTC counter, waking the MCU at the correct time.

---

## Configuration

Edit `Core/Inc/lora_app.h` before flashing:

| Macro | Default | Description |
|-------|---------|-------------|
| `CURRENT_NODE_TYPE` | `NODE_TYPE_RELAY` | Selects relay firmware variant |
| `MY_RELAY_ID` | `0x03` | Unique 1-byte ID of this relay |
| `MANAGED_SENSOR_LIST` | `{0xFA, 0xFE, 0xFD, 0xFC}` | Sensor IDs this relay will manage |
| `MANAGED_SENSOR_COUNT` | `3` | Must equal the number of entries in `MANAGED_SENSOR_LIST` — update together |
| `DEFAULT_TOTAL_CYCLE` | `25` | Default cycle length in seconds (overridden by gateway) |
| `RELAY_ACK_WINDOW_MS` | `1000` | Duration of Task 1 (send ACKs) |
| `RELAY_RX_WINDOW_MS` | `8000` | Duration of Task 2 (listen window) |
| `RELAY_GW_WINDOW_MS` | `1000` | Duration of Task 3 (forward to gateway) |

---

## Build and Flash

1. Open in **STM32CubeIDE**.
2. Set `MY_RELAY_ID` and `MANAGED_SENSOR_LIST` in `Core/Inc/lora_app.h`.
3. Build with **Project -> Build All**.
4. Flash via ST-Link.

Debug output is available on UART2 at 115200 baud.
