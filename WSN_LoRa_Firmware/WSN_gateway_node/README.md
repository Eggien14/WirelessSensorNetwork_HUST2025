# WSN_gateway_node

## Overview

This project contains the firmware for the **Gateway Node (STM32 side)** in the WSN25 Wireless Sensor Network. The gateway node sits at the top of the LoRa network hierarchy. It is the single point of contact between the LoRa radio network and the IP network.

The STM32 gateway has two communication interfaces:
- **LoRa radio (downlink):** receives sensor data aggregated by relay nodes, manages relay registration, and broadcasts configuration commands to all relays.
- **UART (uplink):** communicates with an ESP32 companion board (`WSN_gateway_forward`) that bridges the UART traffic to an MQTT broker over WiFi.

The gateway does not sleep. It runs continuously in an event-driven loop, responding to LoRa packets from relays and configuration commands arriving from the ESP32 via UART.

The firmware runs on an **STM32F103C8T6** microcontroller, developed with STM32CubeIDE.

---

## Hardware

| Component | Description |
|-----------|-------------|
| MCU | STM32F103C8T6 (Blue Pill) |
| Radio | SX1278 LoRa module, connected via SPI1 |
| Serial uplink | UART2 connected to the ESP32 board (TX2/RX2) at 115200 baud |
| Debug | UART2 also used for printf debug output (shared with uplink) |

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
WSN_gateway_node/
|-- Core/
|   |-- Inc/
|   |   |-- main.h          # Pin definitions, buffer sizes, global includes
|   |   |-- lora_app.h      # Protocol constants, frame structures, function declarations
|   |   |-- sx1278_lora.h   # SX1278 driver interface
|   |   |-- gpio.h          # HAL GPIO init declarations
|   |   |-- spi.h           # HAL SPI1 init declarations
|   |   |-- usart.h         # HAL UART2 init declarations
|   |   |-- rtc.h           # HAL RTC init declarations
|   |   `-- tim.h           # HAL TIM1/TIM4 init declarations
|   |-- Src/
|   |   |-- main.c          # Application entry point and event loop
|   |   |-- lora_app.c      # LoRa application logic (relay management + data processing)
|   |   |-- sx1278_lora.c   # SX1278 low-level driver
|   |   |-- gpio.c          # GPIO peripheral initialisation
|   |   |-- spi.c           # SPI1 peripheral initialisation
|   |   |-- usart.c         # UART2 peripheral initialisation and interrupt receive
|   |   |-- rtc.c           # RTC peripheral initialisation
|   |   |-- tim.c           # TIM1/TIM4 peripheral initialisation
|   |   |-- stm32f1xx_hal_msp.c  # HAL MSP low-level init
|   |   |-- stm32f1xx_it.c       # Interrupt handlers (DIO0 EXTI, UART RX, RTC)
|   |   |-- syscalls.c      # Newlib syscall stubs (also implements printf -> UART)
|   |   `-- sysmem.c        # Heap memory configuration
|   `-- Startup/
|       `-- startup_stm32f103c8tx.s  # ARM Cortex-M3 startup assembly
|-- Drivers/
|   |-- CMSIS/              # ARM CMSIS and STM32F1 device headers
|   `-- STM32F1xx_HAL_Driver/  # STM32 HAL source and headers
`-- WSN_gateway_node.ioc    # STM32CubeMX configuration file
```

---

## File Descriptions

### `Core/Inc/lora_app.h`
Shared protocol header (identical across all three STM32 firmware projects). For this project, `CURRENT_NODE_TYPE` is set to `NODE_TYPE_GATEWAY`. Key gateway-specific items:

- `Gateway_Relay_List_t`  struct maintaining a list of up to `MAX_RELAY_QUEUE` (20) registered relay entries, each storing a relay ID and a `last_seen` timestamp.
- `LoRaApp_Gateway_Init()`, `LoRaApp_Gateway_RxProcessing()`, `LoRaApp_Gateway_Send_RL_Queue()`, `LoRaApp_Gateway_ProcessConfigCommand()`  function declarations for the gateway application layer.

### `Core/Src/main.c`
Application entry point. It initialises hardware (GPIO, SPI1, TIM1/TIM4, RTC, UART2), initialises the SX1278 radio in continuous RX mode, and starts byte-by-byte UART interrupt reception. The main loop then runs three concurrent event handlers:

1. **LoRa RX handler:** fires when `loraRxDoneFlag` is set by the DIO0 interrupt, calls `LoRaApp_Gateway_RxProcessing()`.
2. **UART command handler:** fires when `cmdReadyFlag` is set (newline received from ESP32), calls `LoRaApp_Gateway_ProcessConfigCommand()`.
3. **Periodic queue broadcast:** every 5 seconds, calls `LoRaApp_Gateway_Send_RL_Queue()` to report registered relays over UART.

UART reception uses interrupt-driven single-byte mode (`HAL_UART_Receive_IT` with size=1). Each received byte is appended to `uartRxBuffer`. A `\r` or `\n` byte sets `cmdReadyFlag` and copies the complete line to `cmdBuffer` for processing.

### `Core/Src/lora_app.c`
All gateway application logic, compiled with `CURRENT_NODE_TYPE == NODE_TYPE_GATEWAY`. Key functions:

- `LoRaApp_Gateway_Init()`  resets the `Gateway_Relay_List_t`. Called once at startup.
- `LoRaApp_Gateway_RxProcessing()`  dispatches incoming LoRa packets by function code:
  - `FUNC_CODE_RL_REG_ADV` (0x06): a relay is announcing its presence. Adds it to `gw_relay_list` if new; updates `last_seen` if already known.
  - `FUNC_CODE_RL_DATA` (0x04): sensor data aggregated by a relay. Parses the relay ID and all sensor entries, then prints the complete record to UART in the format `DATA,0xRR,0xSS,temp,hum,soil,...\r\n` for the ESP32 to forward.
- `LoRaApp_Gateway_Send_RL_Queue()`  periodically prints the ADV roster over UART in the format `ADV,0xRR,0xRR,...\r\n` so the ESP32 can publish it to the MQTT `Advertise` topic.
- `LoRaApp_Gateway_ProcessConfigCommand()`  parses a configuration string received from the ESP32 over UART (format: `total_cycle,ID1,dt1,ID2,dt2,...`), assembles a `GW_REG_ACK` (0x07) broadcast frame, and transmits it over LoRa 5 times. This broadcasts updated timing parameters to all relays simultaneously.

---

## Data Flow

```
Relay LoRa network                     IP network (via ESP32)
      |                                       |
      | RL_REG_ADV (0x06)                     |
      |-----------> [GW RxProcessing]         |
      |              add to relay list        |
      |              (every 5s) print         |
      |              "ADV,0x01,0x02,..."  --->|---> UART ---> ESP32 ---> MQTT "Advertise"
      |                                       |
      | RL_DATA (0x04)                        |
      |-----------> [GW RxProcessing]         |
      |              parse sensors            |
      |              print                    |
      |              "DATA,0x01,0x01,..."  -->|---> UART ---> ESP32 ---> MQTT "Data"
      |                                       |
      |           <-- UART "60,0x01,30,..." <-|<--- UART <--- ESP32 <--- MQTT "Cycle"
      |           [ProcessConfigCommand]      |
      |           build GW_REG_ACK (0x07)     |
      |<---------- LoRa broadcast x5          |
```

---

## Network Protocol

### Relay Registration Phase (Gateway perspective)

The gateway passively accepts registration advertisements from any relay at any time. There is no negotiation  it simply keeps an internal list. The registration broadcast (`GW_REG_ACK`) is not sent in direct response to individual ADV messages. Instead, it is triggered by the ESP32 forwarding a configuration command from the local server.

**GW_REG_ACK frame format (Gateway -> all Relays, LoRa broadcast):**
```
Byte 0:     func_code  = 0x07
Byte 1:     total_cycle_H  (high byte of uint16)
Byte 2:     total_cycle_L  (low byte)
Byte 3:     count          (number of relay entries)
For each relay (3 bytes):
  Byte n+0: relay_id
  Byte n+1: delta_t_H  (high byte of uint16, wakeup offset in seconds)
  Byte n+2: delta_t_L  (low byte)
```

The gateway broadcasts this frame 5 times to maximise reliability. All relays in range receive it simultaneously; each relay scans the list for its own ID to extract its assigned wakeup offset.

### Report Phase (Gateway perspective)

The gateway runs in continuous RX mode and has no duty cycle of its own. When a relay transmits its `RL_DATA` frame:

1. The DIO0 pin fires an external interrupt, setting `loraRxDoneFlag`.
2. The main loop reads the packet via `LoRa_receive()`.
3. `LoRaApp_Gateway_RxProcessing()` parses the relay ID and iterates through all sensor entries (6 bytes each).
4. Each sensor reading is printed to UART in CSV format immediately.

**RL_DATA parsing (received from Relay):**
```
Byte 0: func_code  = 0x04
Byte 1: relay_id
Byte 2: sensor_count
For each sensor (6 bytes):
  Byte n+0: sensor_id
  Byte n+1-2: temp  (int16, value/10 = Celsius)
  Byte n+3-4: hum   (uint16, value/10 = %RH)
  Byte n+5:   soil  (uint8, %)
```

The resulting UART output printed for the ESP32:
```
DATA,0x01,0xFA,25.5,65.2,45,0xFE,26.1,64.8,44
```

### ADV Periodic Report (Gateway -> ESP32)

Every 5 seconds, the gateway prints the list of relay IDs it has seen to UART:
```
ADV,0x01,0x02,0x03
```

This is parsed by the ESP32 and published to the MQTT `Advertise` topic.

### Configuration Command (ESP32 -> Gateway)

When the local server publishes a new configuration to the MQTT `Cycle` topic, the ESP32 forwards it verbatim over UART. The gateway parses this string and constructs a `GW_REG_ACK` broadcast:

```
UART received from ESP32: "120,0x01,0,0x02,30,0x03,60"
       ^                     ^     ^  ^    ^
       total_cycle=120s     R1 dt=0 R2 dt=30 R3 dt=60
```

The gateway also clears its relay list on every new configuration command (the relays will re-register on the next cycle).

---

## UART Protocol (STM32 <-> ESP32)

The UART channel carries newline-terminated ASCII strings in both directions.

| Direction | Format | Example |
|-----------|--------|---------|
| STM32 -> ESP32 | `ADV,0xID1,0xID2,...\r\n` | `ADV,0x01,0x03\r\n` |
| STM32 -> ESP32 | `DATA,0xRL,0xSS,T,H,S,...\r\n` | `DATA,0x01,0xFA,25.5,65.2,45\r\n` |
| ESP32 -> STM32 | `total_cycle,0xRL,dt,...\r\n` | `120,0x01,0,0x02,30\r\n` |

Node IDs are printed and parsed as hexadecimal strings (`0x01`, `0xFA`, etc.) to maintain consistency with the format used by the local server.

---

## Configuration

Edit `Core/Inc/lora_app.h` before flashing:

| Macro | Default | Description |
|-------|---------|-------------|
| `CURRENT_NODE_TYPE` | `NODE_TYPE_GATEWAY` | Selects gateway firmware variant |
| `MY_GATEWAY_ID` | `0x00` | Gateway ID (informational, not transmitted) |
| `MAX_RELAY_QUEUE` | `20` | Maximum number of relays tracked simultaneously |

**LoRa radio settings** (in `main.c`, `initialize_lora()`): identical to sensor and relay nodes  433 MHz, SF7, BW 125 kHz, CR 4/5, 20 dBm.

---

## Build and Flash

1. Open in **STM32CubeIDE**.
2. Build with **Project -> Build All**.
3. Flash via ST-Link.

The gateway does not require node ID configuration beyond the `MY_GATEWAY_ID` constant (which is not transmitted over radio). Connect UART2 to the ESP32 `WSN_gateway_forward` board before powering on.
