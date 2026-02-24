# WSN25 - Local IoT Server

## Overview

This directory contains the local backend server for the WSN25 Wireless Sensor Network project. The server is responsible for collecting real-time environmental data (temperature, air humidity, soil moisture) from a network of sensor nodes deployed over a 100-hectare agricultural field.

The server acts as the central hub between the LoRa-based wireless hardware network and the end-user web dashboard. It receives sensor data through MQTT, persists it to CSV files, exposes a REST API, and serves a browser-based management interface.

---

## Directory Structure

```
Full_local/
|-- app.py                  # Main Flask application and entry point
|-- config.py               # All system-wide configuration constants
|-- database_manager.py     # CSV database abstraction layer
|-- mqtt_handler.py         # MQTT client wrapper
|-- cloud_update.py         # Cloud synchronization module (ThingsBoard)
|-- requirements.txt        # Python package dependencies
|-- README.md               # This document
|-- Database/
|   |-- ADV.csv             # Relay-sensor topology table
|   |-- CYCLE.csv           # Per-relay measurement cycle configuration
|   |-- DATA.csv            # Latest sensor readings (current state)
|   |-- OLD_DATA.csv        # Full historical sensor readings (append-only)
|   |-- MSG.csv             # Raw MQTT message log (audit trail)
|   `-- system_state.json   # Persisted runtime state
`-- static/
    |-- index.html          # Single-page web dashboard
    |-- app.js              # Frontend JavaScript logic
    `-- style.css           # Dashboard stylesheet
```

---

## File Descriptions

### Python Source Files

#### `app.py`
The main application file and server entry point. It wires all components together:

- Initialises the Flask application with CORS support.
- Instantiates `DatabaseManager` and `MQTTHandler`.
- Defines the `SystemState` class, which tracks whether the system is running, which relays are selected, and what the total cycle duration is. This state is persisted to `system_state.json` so it survives server restarts.
- Registers MQTT callbacks: `handle_advertise()` processes incoming relay registrations; `handle_data()` processes incoming sensor readings.
- Declares all REST API endpoints (see API section below).
- On startup, initialises MQTT only once (avoids duplication caused by Flask's reloader).

#### `config.py`
Centralised configuration file. All constants used across the application are defined here, including:

- MQTT broker address, port, and keepalive interval.
- MQTT topic names (`Advertise`, `Cycle`, `Data`).
- Flask server host, port, and debug flag.
- Relative paths to the five database files.
- Default threshold values for temperature, air humidity, and soil moisture alerts.

Modifying this file is the only change required to adapt the server to a different environment.

#### `database_manager.py`
Provides the `DatabaseManager` class, which serves as the data access layer for all five CSV files. Key design decisions:

- Each CSV file has its own `threading.Lock()` to prevent race conditions when concurrent MQTT messages and HTTP requests access the same file simultaneously.
- `ADV.csv` is always rewritten (not appended) to enforce uniqueness of `relay_id`.
- `DATA.csv` stores only the latest reading per sensor; older values are overwritten. The unique key is `sensor_id`.
- `OLD_DATA.csv` is append-only and stores the complete measurement history.
- `MSG.csv` supports deduplication: a message is only accepted if it does not match the topic, payload, and timestamp of any of the five most recent entries.
- `get_sensor_history()` merges data from both `OLD_DATA.csv` and `DATA.csv`, de-duplicates the results, and supports filtering by time range.
- `_sort_data_by_relay()` reorders `DATA.csv` rows so that each relay row appears immediately before the sensor rows it manages, improving readability.

#### `mqtt_handler.py`
Provides the `MQTTHandler` class, a thin wrapper over `paho-mqtt`. Responsibilities:

- Manages the connection lifecycle with the local Mosquitto broker.
- Runs the MQTT network loop in a background thread (`loop_start()`).
- Implements a first-level deduplication guard: if the same payload arrives on the same topic within two seconds, the second message is silently discarded before it reaches `app.py`.
- Exposes `subscribe_advertise()`, `subscribe_data()`, and `publish_cycle()` as the public interface used by `app.py`.

#### `cloud_update.py`
Reserved for the ThingsBoard cloud synchronisation module. When implemented, this module will forward sensor data from the local database to a ThingsBoard MQTT endpoint using the gateway API format, enabling remote monitoring via the ThingsBoard web interface.

---

### Database Files

#### `ADV.csv`
Stores the network topology: which sensor nodes are managed by each relay.

```
relay_id,sensor_ids
0x01,"0xFA,0xFB,0xFC"
0x02,"0xFD,0xFE"
```

- Populated automatically when a relay publishes to the `Advertise` topic.
- Updated automatically when sensor data arrives and reveals a new sensor-to-relay mapping.
- Each `relay_id` appears exactly once.

#### `CYCLE.csv`
Stores the per-relay measurement interval (`delta_t` in seconds).

```
relay_id,delta_t
0x01,30
0x02,45
```

- Updated via the `/api/update_cycle` endpoint.
- Used to construct the `Cycle` MQTT message when the system is started.
- Default value is 60 seconds if a relay has no explicit configuration.

#### `DATA.csv`
Stores the most recent sensor reading for every known sensor node.

```
relay_id,sensor_id,temp,humid,soil,timestamp
0x01,0x01,25.5,65.2,45.0,2026-01-26 10:30:00
0x01,0xFA,26.1,64.8,44.5,2026-01-26 10:30:00
```

- Overwritten on every new MQTT `Data` message; only the latest reading is kept.
- The primary key is `sensor_id` (globally unique across all relays).
- A relay node that also acts as a sensor will appear with `relay_id == sensor_id`.

#### `OLD_DATA.csv`
An append-only log of all historical sensor readings. Shares the same column schema as `DATA.csv`. Used to render time-series charts in the web dashboard.

#### `MSG.csv`
An append-only log of every MQTT message received by the server, used for debugging and deduplication.

```
topic,message,timestamp
Advertise,"0x01, 0x02",2026-01-26 10:00:00
Data,"0x01,0xFA,25.5,65.2,45.0",2026-01-26 10:30:00
```

#### `system_state.json`
A small JSON file persisting the runtime state of the server.

```json
{
  "running": true,
  "selected_relays": ["R001", "R002"],
  "total_cycle": 120
}
```

Loaded on startup so that the server resumes its previous configuration after a restart.

---

### Static Frontend Files

#### `index.html`
Defines the structure of the single-page dashboard. Contains three view panels toggled by JavaScript: the Manager view, the General (overview) view, and the Sensor Detail view.

#### `app.js`
All frontend logic. Communicates with the Flask server exclusively via the REST API. Key behaviours:

- Polls `/api/data` and `/api/status` every five seconds for live updates.
- Renders interactive threshold-highlighting tables in the General view.
- Renders time-series charts (using Chart.js) in the Detail view, with configurable time-range filters.
- Handles start/stop, cycle configuration, threshold configuration, and relay deletion actions.

#### `style.css`
Responsive CSS styling for the dashboard, including card-based layout, alert colour coding (normal, warning, danger), and chart containers.

---

## System Architecture

```
Field hardware (LoRa)
       |
       | RF / WiFi
       v
 MQTT Broker (Mosquitto, localhost:1883)
       |
       +-- topic: Advertise --> handle_advertise() --> ADV.csv
       |
       +-- topic: Data      --> handle_data()      --> DATA.csv
       |                                            --> OLD_DATA.csv
       |                                            --> ADV.csv (sensor mapping)
       |
       +-- topic: Cycle     <-- publish_cycle()    <-- /api/start
       |
 Flask Server (port 5000)
       |
       +-- REST API (/api/*)
       |
       +-- Static files (/, /static/*)
       |
 Web Dashboard (browser)
```

---

## MQTT Protocol

The server communicates with the field hardware (gateway) over three MQTT topics.

### Advertise (Gateway to Server)

Relays broadcast their presence when they come online.

```
Topic:   Advertise
Payload: "0x01, 0x02, 0x03"
```

The server parses the comma-separated list of relay IDs (hex strings, whitespace-tolerant) and registers any new relays in `ADV.csv`. IDs are stored exactly as received — `0x01`, `0xFA`, etc.

### Data (Gateway to Server)

The gateway publishes one `Data` message per relay data frame received from the LoRa network.

```
Topic:   Data
Payload: "relay_id,sensor_id,temp,humid,soil,relay_id,sensor_id,temp,humid,soil,..."
```

Each sensor entry occupies exactly five consecutive fields: `relay_id, sensor_id, temp, humid, soil`. The server parser requires `len(fields) % 5 == 0`; non-conforming messages are discarded with a warning logged.

Example (relay `0x01` forwarding data from sensor `0xFA`):
```
Data: "0x01,0xFA,25.50,65.20,45.00"
```

Node IDs throughout are hex strings (`0x01`, `0xFA`, etc.) matching the format used by the gateway firmware.

### Cycle (Server to Gateway)

When the user presses Start in the dashboard, the server publishes the measurement schedule for all selected relays.

```
Topic:   Cycle
Payload: "T,relay_id,delta_t,relay_id,delta_t,..."
```

- `T` is the total cycle duration in seconds, applied to all relays.
- `delta_t` is the individual wakeup offset for each relay in seconds.

Example:
```
Cycle: "120,0x01,0,0x02,30,0x03,60"
```

The ESP32 bridge (`WSN_gateway_forward`) receives this payload verbatim and forwards it over UART to the STM32 gateway, which parses it and broadcasts `GW_REG_ACK` (0x07) to all relays.

---

## REST API Reference

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/` | Serve the web dashboard |
| GET | `/api/relays` | List all relays and their sensors |
| DELETE | `/api/relay/<relay_id>` | Remove a relay (only when system is stopped) |
| GET | `/api/data` | Get the latest reading of every sensor |
| GET | `/api/sensor/<relay_id>/<sensor_id>` | Get history for a specific sensor |
| POST | `/api/update_cycle` | Set `delta_t` for a relay |
| POST | `/api/start` | Start the system (publish Cycle message) |
| POST | `/api/stop` | Stop the system |
| GET | `/api/status` | Get current system state |
| GET | `/api/thresholds` | Get current alert thresholds |
| POST | `/api/thresholds` | Update alert thresholds |

### Selected Endpoint Details

**POST /api/start**
```json
Request:  { "selected_relays": ["0x01", "0x02"], "total_cycle": 120 }
Response: { "success": true, "running": true, "selected_relays": ["0x01","0x02"], "total_cycle": 120 }
```

**GET /api/sensor/0x01/0xFA?time_range=hour**
```json
Response: {
  "success": true,
  "relay_id": "0x01",
  "sensor_id": "0xFA",
  "is_relay": false,
  "history": [ { "temp": "25.5", "humid": "65.2", "soil": "45.0", "timestamp": "..." } ]
}
```

Available `time_range` values: `default` (last 20 entries), `minute`, `hour`, `day`, `month`.

**POST /api/thresholds**
```json
{
  "temp_min": 15.0, "temp_max": 35.0,
  "humid_min": 40.0, "humid_max": 80.0,
  "soil_min": 30.0, "soil_max": 70.0
}
```

---

## Setup and Installation

### Prerequisites

- Python 3.8 or later
- Mosquitto MQTT broker running on `localhost:1883`

Install Mosquitto on Windows: Download from https://mosquitto.org/download/ and start the service.

Install Mosquitto on Linux:
```
sudo apt install mosquitto mosquitto-clients
sudo systemctl enable --now mosquitto
```

### Install Python Dependencies

```
pip install -r requirements.txt
```

Dependencies: `Flask 3.0.0`, `Flask-CORS 4.0.0`, `paho-mqtt 1.6.1`

### Run the Server

```
python app.py
```

The server will start on `http://0.0.0.0:5000`. Open `http://localhost:5000` in a browser to access the dashboard.

The `Database/` directory and all CSV files are created automatically on first run if they do not already exist.

---

## Deduplication Strategy

Duplicate MQTT messages are suppressed at two independent layers:

1. **MQTTHandler layer:** Any message with an identical payload arriving on the same topic within two seconds of the previous one is dropped before reaching the application logic.

2. **DatabaseManager layer:** Before writing to `DATA.csv`, the server checks the last five entries in `MSG.csv`. If a record with the same topic, payload, and timestamp already exists, the message is treated as a duplicate and discarded.

This two-layer approach handles both rapid redelivery (network-level duplicates) and slow duplicates caused by broker reconnection events.

---

## Configuration Reference

All configurable parameters are in `config.py`:

| Parameter | Default | Description |
|-----------|---------|-------------|
| `MQTT_BROKER` | `"localhost"` | MQTT broker hostname or IP |
| `MQTT_PORT` | `1883` | MQTT broker port |
| `MQTT_KEEPALIVE` | `60` | MQTT keepalive interval (seconds) |
| `FLASK_HOST` | `"0.0.0.0"` | Listen address (0.0.0.0 = all interfaces) |
| `FLASK_PORT` | `5000` | HTTP server port |
| `FLASK_DEBUG` | `True` | Enable Flask debug mode (disable in production) |
| `DEFAULT_THRESHOLDS` | see file | Default min/max values for temp, humid, soil |

---

## Troubleshooting

**Server starts but MQTT shows "Connection refused"**
Verify that Mosquitto is running: `netstat -an | findstr 1883` (Windows) or `ss -tlnp | grep 1883` (Linux). Start or restart the broker if needed.

**No data appears in the dashboard**
Confirm the gateway is publishing to the correct broker address and that topic names match exactly (they are case-sensitive). Use `mosquitto_sub -t "#" -v` to monitor all incoming messages and verify the payload format.

**Duplicate rows appear in ADV.csv**
This can happen after an abnormal server shutdown during a write operation. Delete `ADV.csv`; the file will be recreated cleanly from the next `Advertise` message.

**OLD_DATA.csv grows very large**
`OLD_DATA.csv` is append-only and will grow indefinitely. Archive or truncate it periodically in environments with high message frequency.
