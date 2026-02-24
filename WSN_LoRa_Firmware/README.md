# WSN_LoRa_Firmware

Firmware for all embedded nodes in the WSN25 LoRa network. The network implements a three-tier cluster-tree topology: **Sensor Nodes** collect field measurements, **Relay Nodes** aggregate data per cluster, and the **Gateway** (a two-board design) bridges the LoRa network to the MQTT broker over WiFi.

---

## Sub-projects

| Directory | MCU | Role | README |
|-----------|-----|------|--------|
| `WSN_sensor_node/` | STM32F103C8T6 | Leaf node  measures temperature, air humidity, and soil moisture; transmits on a TDMA schedule; sleeps between cycles | [README](WSN_sensor_node/README.md) |
| `WSN_relay_node/` | STM32F103C8T6 | Cluster head  receives sensor data, assigns TDMA slots, aggregates and forwards data to the gateway; sleeps between cycles | [README](WSN_relay_node/README.md) |
| `WSN_gateway_node/` | STM32F103C8T6 | LoRa network root  receives relay data, manages relay registration, outputs structured ASCII frames to the ESP32 companion board via UART; always-on | [README](WSN_gateway_node/README.md) |
| `WSN_gateway_forward/` | ESP32 | UART-to-MQTT bridge  translates ASCII frames from the STM32 gateway to MQTT topics and forwards configuration commands in the opposite direction; always-on | [README](WSN_gateway_forward/README.md) |

The gateway is a two-board design. The STM32 board (`WSN_gateway_node`) handles all LoRa radio communication. The ESP32 board (`WSN_gateway_forward`) handles all WiFi and MQTT communication. The two boards communicate over UART at 115200 baud. All three STM32 projects share a single codebase with conditional compilation controlled by `CURRENT_NODE_TYPE` in `lora_app.h`.

---

## Network Architecture

```
Field                                              LAN
          
Sensor 0xFA 
Sensor 0xFB  LoRa  Relay 0x01 
Sensor 0xFC                         
                                        LoRa  STM32 Gateway  UART  ESP32  WiFi/MQTT  Broker
Sensor 0xFD                         
Sensor 0xFE  LoRa  Relay 0x02 
Sensor 0xFF 
```

All LoRa links use identical radio parameters: **433 MHz, SF7, BW 125 kHz, CR 4/5, 20 dBm**.

---

## Communication Protocol

The protocol is fully custom, implemented over the raw SX1278 LoRa physical layer. It defines two operational phases per node lifetime.

### Function Codes

| Code | Mnemonic | Direction | Description |
|------|----------|-----------|-------------|
| `0x01` | `REG_ADV` | Sensor  Relay | Sensor registration request |
| `0x02` | `REG_ACK` | Relay  Sensor | TDMA slot + cycle assignment |
| `0x03` | `SS_DATA` | Sensor  Relay | Sensor measurement frame |
| `0x04` | `RL_DATA` | Relay  Gateway | Aggregated sensor data from one relay cluster |
| `0x05` | `GW_ACK` | Gateway  Relay | Delivery acknowledgement |
| `0x06` | `RL_REG_ADV` | Relay  Gateway | Relay registration request |
| `0x07` | `GW_REG_ACK` | Gateway  All Relays | Broadcast: cycle period + per-relay wakeup offsets |

### Phase 1  Registration

Nodes perform registration once at power-on before entering the cyclic report phase.

**Relay  Gateway (`0x06` / `0x07`):**

```
Relay    [0x06 | relay_id | 0x00]                             3 bytes, repeated until config received
Gateway  [0x07 | cycle_H | cycle_L | count | id | dt_H | dt_L | ...]   broadcast  5
```

The `GW_REG_ACK` frame carries the global cycle period and an individual wakeup offset (`delta_t`) for every registered relay. Each relay reads its own offset and sleeps `delta_t` seconds at the start of each cycle, staggering relay-to-gateway transmissions to avoid collision.

**Sensor  Relay (`0x01` / `0x02`):**

```
Sensor  [0x01 | sensor_id | target_relay_id]                  3 bytes, repeated until ACK
Relay   [0x02 | relay_id | sensor_id | tdma_slot | cycle_H | cycle_L | 0x00]   7 bytes
```

The relay assigns each sensor a TDMA slot index. After receiving the ACK, the sensor sleeps for one full cycle to align its transmit window with the relay.

### Phase 2  Report (one cycle)

```
[Cycle start]
    
     Relay wakes after delta_t offset
    
     Relay Task 1 (1 s):   Send queued REG_ACKs for sensors registered in previous cycle
    
     Relay Task 2 (8 s):   Listen window
            On 0x01:  Queue new sensor for ACK
            On 0x03:  Store sensor measurement
    
      [Sensors wake after TDMA offset = 1500 + slot  100 ms]
           Send SS_DATA  2    Relay stores reading
    
     Relay Task 3 (1 s):   Send RL_DATA to Gateway  wait for GW_ACK (0x05)
            Gateway prints DATA,0xRL,0xSS,T,H,S,... to UART  ESP32  MQTT
    
     RTC STOP sleep  (TOTAL_CYCLE_SEC  10 s)
```

### Message Frame Reference

| Frame | Size | Layout |
|-------|------|--------|
| `REG_ADV` (0x01) | 3 B | `func \| sensor_id \| target_relay_id` |
| `REG_ACK` (0x02) | 7 B | `func \| relay_id \| sensor_id \| tdma_slot \| cycle_H \| cycle_L \| 0x00` |
| `SS_DATA` (0x03) | 8 B | `func \| sensor_id \| relay_id \| temp_H \| temp_L \| hum_H \| hum_L \| soil` |
| `RL_DATA` (0x04) | variable | `func \| relay_id \| count \| [sensor_id \| temp_H \| temp_L \| hum_H \| hum_L \| soil]  N` |
| `GW_ACK` (0x05) | 3 B | `func \| relay_id \| 0x00` |
| `RL_REG_ADV` (0x06) | 3 B | `func \| relay_id \| 0x00` |
| `GW_REG_ACK` (0x07) | variable | `func \| cycle_H \| cycle_L \| count \| [relay_id \| dt_H \| dt_L]  N` |

`temp` and `hum` are `int16` / `uint16` with one decimal digit of precision (value / 10 = physical unit). `soil` is `uint8` in percent.

### Default Timing Constants

| Constant | Value | Description |
|----------|-------|-------------|
| `DEFAULT_TOTAL_CYCLE` | 25 s | Full cycle period (overridable by server) |
| `SENSOR_TDMA_BASE_MS` | 1500 ms | Base TDMA offset for slot 0 |
| `SENSOR_TDMA_SLOT_MS` | 100 ms | Per-slot increment |
| `SENSOR_TX_WINDOW_MS` | 3000 ms | Sensor transmit window |
| `SENSOR_MEASURE_WINDOW_MS` | 3000 ms | Sensor measurement window |
| `SENSOR_MEASURE_CYCLE` | 3 | Measure once every N report cycles |
| `RELAY_RX_WINDOW_MS` | 8000 ms | Relay sensor-listening window |
| `RELAY_GW_WINDOW_MS` | 1000 ms | Relay-to-gateway transmit window |
| `REG_TIMEOUT_MS` | 2000 ms | Registration attempt timeout |

---

## Hardware Summary

### Common to all STM32 nodes

| Component | Detail |
|-----------|--------|
| MCU | STM32F103C8T6 (Blue Pill) |
| Radio | SX1278 LoRa module, SPI1 |
| Radio config | 433 MHz, SF7, BW 125 kHz, CR 4/5, 20 dBm, preamble 8 |
| SPI pins | CS=PB0, RST=PB1, DIO0=PA4 (EXTI4) |
| Debug UART | UART2, 115200 baud |

### Sensor node additions

| Component | Detail |
|-----------|--------|
| DHT22 | Air temperature + humidity |
| Soil moisture probe | Resistive probe via ADC1 |
| Power | LiPo battery + TP4056 charge circuit (YDL 7565121 3.7 V 8000 mAh) |

### Gateway additions

| Component | Detail |
|-----------|--------|
| ESP32 DevKit | WiFi + MQTT bridge |
| Gateway UART | STM32 TX (PA2)  ESP32 RX (GPIO16), STM32 RX (PA3)  ESP32 TX (GPIO17) |

---

## Development Toolchains

| Project | Tool | Framework |
|---------|------|-----------|
| `WSN_sensor_node` | STM32CubeIDE | STM32 HAL |
| `WSN_relay_node` | STM32CubeIDE | STM32 HAL |
| `WSN_gateway_node` | STM32CubeIDE | STM32 HAL |
| `WSN_gateway_forward` | PlatformIO | Arduino (espressif32) |

---

## Deployment Order

1. Flash and power on **`WSN_gateway_forward`** (ESP32). Confirm WiFi and MQTT connection.
2. Flash and power on **`WSN_gateway_node`** (STM32). Confirm UART activity.
3. Flash and power on all **`WSN_relay_node`** boards. Each enters registration mode and awaits `GW_REG_ACK`.
4. From the server dashboard, send an initial cycle configuration (MQTT `Cycle` topic). The gateway broadcasts `GW_REG_ACK` and relays complete registration.
5. Flash and power on all **`WSN_sensor_node`** boards. Each broadcasts `REG_ADV` until acknowledged by a relay.

After step 5, all nodes are registered and cyclic reporting begins automatically.
