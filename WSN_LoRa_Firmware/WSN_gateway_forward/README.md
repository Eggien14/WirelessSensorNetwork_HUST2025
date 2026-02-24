# WSN_gateway_forward

## Overview

This project contains the firmware for the **ESP32 companion board** in the WSN25 gateway. It acts as a transparent UART-to-MQTT bridge between the STM32 gateway node (`WSN_gateway_node`) and the local MQTT broker running on the server host.

Its sole responsibility is to:
- Read newline-terminated ASCII messages from the STM32 via UART, parse the message type, and publish the payload to the appropriate MQTT topic.
- Subscribe to the MQTT `Cycle` topic and forward any received configuration strings directly to the STM32 via UART.

The board has no state machine of its own. All protocol logic lives in the STM32 firmware. The ESP32 only translates between the two transport layers.

Developed with **PlatformIO** using the **Arduino** framework.

---

## Hardware

| Component | Description |
|-----------|-------------|
| MCU | ESP32 (any standard DevKit) |
| Uplink | WiFi 802.11 b/g/n  connects to local LAN |
| Downlink | UART port (Serial2): RX=GPIO16, TX=GPIO17 at 115200 baud |
| MQTT broker | Connects to mosquitto instance at 172.20.149.45:1883 |

---

## Directory Structure

```
WSN_gateway_forward/
|-- src/
|   `-- main.cpp        # All application logic (WiFi, MQTT, UART bridge)
|-- include/
|   `-- README          # PlatformIO placeholder, no project headers
|-- lib/
|   `-- README          # PlatformIO placeholder, no local libraries
|-- platformio.ini      # PlatformIO build and upload configuration
`-- README.md
```

---

## File Descriptions

### `src/main.cpp`
The entire application fits in one file. It uses the Arduino `loop()` / `setup()` pattern.

**`setup()`**
- Starts `Serial` (USB, 115200) for debug output.
- Starts `Serial2` (GPIO16/GPIO17, 115200) for STM32 communication.
- Calls `setup_wifi()` to connect to the configured SSID.
- Configures the PubSubClient MQTT instance with broker address and port, and sets `mqttCallback` as the message handler.
- Calls `reconnect_mqtt()` to establish the initial MQTT connection.

**`loop()`**
- Calls `client.loop()` to keep the MQTT connection alive and dispatch incoming messages.
- Calls `reconnect_mqtt_if_needed()` to attempt reconnection every 5 seconds if the broker is unreachable.
- Reads `Serial2` byte by byte into a line buffer. On newline, calls `handleUARTMessage(line)`.

**`handleUARTMessage(line)`**
- Splits the ASCII line on the first comma to separate the command prefix from the payload.
- Calls `forwardToMQTT(command, payload)`.

**`forwardToMQTT(command, payload)`**

| Command prefix | MQTT topic | Payload published |
|----------------|-----------|-------------------|
| `ADV` | `Advertise` | Everything after the first comma |
| `DATA` | `Data` | Everything after the first comma |
| Any other | (ignored) |  |

For example:
```
UART received:    "ADV,0x01,0x02,0x03\r\n"
MQTT published:   topic="Advertise", payload="0x01,0x02,0x03"

UART received:    "DATA,0x01,0xFA,25.5,65.2,45\r\n"
MQTT published:   topic="Data", payload="0x01,0xFA,25.5,65.2,45"
```

**`mqttCallback(topic, payload, length)`**
- Handles messages from subscribed topics.
- If topic is `Cycle`: converts the payload bytes to a null-terminated string and calls `Serial2.println(message)` to forward it verbatim to the STM32. No prefix or transformation is applied.

```
MQTT received:   topic="Cycle", payload="120,0x01,0,0x02,30,0x03,60"
UART sent:       "120,0x01,0,0x02,30,0x03,60\n"
```

**`reconnect_mqtt()` / `reconnect_mqtt_if_needed()`**
- On connection, subscribes to the `Cycle` topic.
- Retries every 5 seconds on failure. If WiFi drops, calls `ESP.restart()` to force a clean reconnection sequence.

---

## MQTT Topics

| Topic | Direction | Format | Description |
|-------|-----------|--------|-------------|
| `Advertise` | Publish | `0xRL,0xRL,...` | List of relay IDs seen by the gateway, broadcast every 5 seconds |
| `Data` | Publish | `0xRL,0xSS,T,H,S,...` | Sensor readings aggregated from one relay |
| `Cycle` | Subscribe | `total_cycle,0xRL,dt,...` | Configuration from server, forwarded to STM32 |

The `Advertise` and `Data` topics are consumed by the local server (`Full_local/mqtt_handler.py`). The `Cycle` topic is published by the local server when it wants to push updated timing configuration to the LoRa network.

---

## Configuration

Edit the constants at the top of `src/main.cpp` before building:

| Constant | Default | Description |
|----------|---------|-------------|
| `WIFI_SSID` | `"..."` | WiFi network name |
| `WIFI_PASSWORD` | `"..."` | WiFi password |
| `MQTT_SERVER` | `"172.20.149.45"` | IP address of the MQTT broker |
| `MQTT_PORT` | `1883` | MQTT broker port |
| `MQTT_CLIENT_ID` | `"esp32_gateway"` | MQTT client identifier |

---

## Build and Flash

Requires [PlatformIO](https://platformio.org/) (VS Code extension or CLI).

```
platformio.ini summary:
  platform  = espressif32
  board     = esp32dev
  framework = arduino
  lib_deps  = knolleary/PubSubClient
```

Build and flash steps:
1. Open the `WSN_gateway_forward` folder in VS Code with the PlatformIO extension installed.
2. Edit WiFi and MQTT credentials in `src/main.cpp`.
3. Click **Build** (or run `pio run`).
4. Click **Upload** (or run `pio run --target upload`).
5. Open the **Serial Monitor** at 115200 baud to verify WiFi and MQTT connection status.

---

## Connection with WSN_gateway_node

Connect UART2 of the STM32 gateway node to Serial2 of the ESP32 **with TX/RX swapped**:

| STM32 UART2 | ESP32 Serial2 |
|-------------|---------------|
| TX (PA2) | RX (GPIO16) |
| RX (PA3) | TX (GPIO17) |
| GND | GND |

Both sides must share a common ground. Both operate at 115200, 8N1.
