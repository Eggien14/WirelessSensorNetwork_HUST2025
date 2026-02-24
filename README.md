# Wireless Sensor Network for 100-Hectare Agricultural Field Monitoring

> **Vietnamese title:** Thiết kế mạng cảm biến không dây giám sát nhiệt độ và độ ẩm trên các cánh đồng diện tích 100 hecta

**Course:** EE4552  Wireless Sensor Network 20251 

**Institution:** School of Electrical and Electronics Engineering, Hanoi University of Science and Technology (HUST)  

**Supervisor:** Assoc. Prof. Lê Minh Thùy  Measurement Engineering & Industrial Informatics Group  

**Team (Group 5):** Nguyễn Trung Kiên (20210500)  Vũ Quang Nhật Hải (20222125)  Nguyễn Anh Tú (20222423) 

**Final Score:** Process 10/10  Final Exam 10/10  **Grade: A+**

**Demo Video:** https://www.youtube.com/watch?v=HvIljYlBAPk&feature=youtu.be
---

## 1. Project Background and Motivation

Vietnam's agricultural sector is increasingly shifting toward large-scale cultivation models, where individual fields can span hundreds of hectares. In this context, manual soil and atmospheric monitoring is financially and operationally impractical: deploying wired infrastructure across such distances is prohibitively expensive, and manual sampling yields data that is both sparse and delayed.

This project addresses the core challenge of large-area environmental monitoring through the design and implementation of a complete, end-to-end **Wireless Sensor Network (WSN)** system. The system enables automatic and continuous collection of temperature, air humidity, and soil moisture measurements, giving farm operators accurate, near-real-time data to support irrigation and crop management decisions.

The primary research emphasis is on the **radio communication layer and network protocol**  specifically, the design of a custom application-layer protocol over LoRa that handles addressing, collision avoidance, and routing in a heterogeneous multi-hop network without any existing protocol stack.

---

## 2. Technical Requirements

The following specifications were defined at project initiation and used as acceptance criteria.

| Requirement | Specification | Achieved |
|-------------|--------------|----------|
| Air temperature measurement | 10 C to 50 C, 0.5 C accuracy, 0.1 C display resolution |  |
| Air humidity measurement | 0100 %RH, 2 %RH accuracy |  |
| Soil moisture measurement | 060 %Vol, 3 %Vol accuracy |  |
| Sampling period | < 60 s per sample |  |
| RF range per node |  100 m to support 100 ha coverage |  |
| Minimum network capacity | 100 sensor nodes |  (by design) |
| End-to-end data latency | < 30 s |  |
| Battery lifetime |  2 years per node via power management |  (by calculation) |
| Remote configuration | Measurement cycle and alert thresholds configurable from server |  |
| Data export | CSV / Excel-compatible data files |  |
| Cloud integration | Real-time synchronization to ThingsBoard cloud |  |

---

## 3. System Architecture

The system is structured as three functional subsystems, delivering an end-to-end IoT pipeline from field sensors to a cloud dashboard.

```
Field Layer (LoRa 433 MHz)              Local Server (LAN)          Cloud
               
Sensor Node                           MQTT Broker
Sensor Node   LoRa  Relay Node     
Sensor Node                             
                                         STM32 Gateway UART ESP32 WiFi Flask Server  ThingsBoard
Sensor Node                                                        
Sensor Node   LoRa  Relay Node                Web Dashboard 
Sensor Node                                         REST API
```

**Subsystem 1  Sensor Nodes:** STM32F103C8T6 microcontrollers with SX1278 LoRa radios, DHT22 temperature/humidity sensors, and resistive soil moisture probes. Nodes operate autonomously on battery power and enter RTC-triggered STOP mode between measurement cycles to minimize power consumption.

**Subsystem 2  Gateway & Protocol:** A multi-hop relay network with a custom LoRa application protocol. Relay nodes aggregate sensor data per cluster. The gateway is a two-board design: an STM32 board handles all LoRa communication; an ESP32 board bridges the UART output from the STM32 to the MQTT broker over WiFi.

**Subsystem 3  Local Server & Dashboard:** A Python/Flask application that subscribes to MQTT, stores readings in CSV files with thread-safe two-layer deduplication, auto-discovers the network topology, and serves a live web dashboard. The server also supports real-time data forwarding to the ThingsBoard cloud platform.

---

## 4. Development Process

The project was executed over **23 weeks**, structured around 6 milestones with detailed Gantt charts maintained throughout.

### Team Responsibilities

| Member | Role | Areas |
|--------|------|--------|
| Nguyễn Trung Kiên (20210500) | Project Lead | Project management, local server development, cloud dashboard, gateway integration, message protocol design  |
| Vũ Quang Nhật Hải (20222125) | Firmware | Sensor node, relay node, gateway firmware; message protocol design; command list definition; presentation slides |
| Nguyễn Anh Tú (20222423) | Hardware & Firmware | Sensor node and relay node firmware; hardware component selection; presentation slides; demo video |

### Development Timeline (6 Milestones)

| Phase | Key Deliverables |
|-------|-----------------|
| W1  Project initiation | Requirement definition, priority matrix, team structure, Gantt chart |
| W2W4  Technology selection & hardware design | Comparative analysis of wireless technologies (BLE, Zigbee, WiFi, LoRa, NB-IoT, LTE); component selection (STM32, SX1278, DHT22, soil probe); PCB design; link budget analysis |
| W5W10  Firmware & protocol development | Custom LoRa application protocol (registration + report phases, TDMA, wakeup offset); sensor/relay/gateway firmware; STM32 sleep mode validation; SX1278 driver implementation |
| W11W15  Server development | Flask server, MQTT handler, CSV database, two-layer deduplication, auto-discovery, web dashboard, ThingsBoard integration |
| W16W20  Integration & testing | Unit testing (sensors, LoRa modules, STM32 power modes), communication tests, end-to-end system integration, multi-node network testing |
| W21W23  Final evaluation & report | Final experimental validation, project report (142 pages), presentation, demo |

### Technology Selection Rationale

The team conducted a systematic comparison of six wireless technologies  Bluetooth Low Energy, Zigbee, WiFi, LoRa, NB-IoT, and LTE  against the 100-hectare, battery-powered, low-latency requirements. **LoRa at 433 MHz** was selected for its long range (>500 m line-of-sight with SX1278 at 20 dBm), sub-mW idle power consumption, license-free spectrum, and full protocol customisation capability. A **cluster-tree topology** was chosen over star and mesh alternatives to balance coverage, capacity, and energy efficiency.

---

## 5. Results

### Experimental Validation

Final testing was conducted in three scenarios covering the complete node lifecycle:

**Scenario 1  Relay network establishment:** Relay nodes successfully advertised to the gateway, appeared as pending devices on the server dashboard, and completed registration upon operator approval. The full relay-to-gateway registration and configuration flow operated correctly end-to-end.

**Scenario 2  Sensor cluster expansion:** Sensor nodes successfully discovered and joined relay clusters. TDMA slot assignment, ACK, and synchronisation all operated correctly. Relay node membership lists updated in real time.

**Scenario 3  End-to-end data flow:** Sensor data transmitted on the assigned TDMA schedule, was aggregated by the relay, forwarded to the gateway, and appeared on the server dashboard with correct values. End-to-end latency remained within the 30-second requirement.

### Quantitative Results

| Metric | Result |
|--------|--------|
| LoRa packet loss rate | < 5% (aided by retransmission logic in firmware) |
| End-to-end latency (sensor  dashboard) | < 30 s |
| Temperature accuracy (DHT22) | 0.5 C (within spec) |
| Air humidity accuracy | 25 %RH (within spec) |
| Soil moisture (dry, in air) | ADC = 4095  0% |
| Soil moisture (in water) | ADC  05  100% |
| STM32 STOP mode wake-up (RTC) | Successful; RAM contents preserved |
| Remote cycle reconfiguration | Operational (MQTT Cycle  gateway broadcast  all relays) |

### Project Self-Assessment (from Chapter 9, Report)

| Requirement | Score |
|-------------|-------|
| Measurement accuracy (temperature, humidity, soil) | 10 / 10 |
| Battery operation and power management | 9 / 10 |
| Physical size and weight | 7 / 10 |
| RF coverage and network communication | 10 / 10 |
| Latency and node capacity (100 nodes, <30 s) | 10 / 10 |
| Software and dashboard interface | 10 / 10 |
| Threshold alerts and remote configuration | 10 / 10 |
| Advanced features (OTA update, adaptive duty cycle) | 0 / 10 (not implemented) |
| Deployment plan and timeline compliance | 10 / 10 |

### Key Technical Achievements

1. **Custom LoRa MAC-layer protocol**  Designed from scratch: node addressing, two-phase registration, TDMA-based collision avoidance at the sensor level, and wakeup-offset staggering at the relay level, all without relying on any existing LoRaWAN or mesh stack.

2. **Power management**  STM32F103C8T6 STOP mode with RTC periodic wake-up validated. Theoretical battery lifetime of  2 years achieved with the YDL 7565121 3.7 V / 8000 mAh Li-ion cell and TP4056 charge circuit.

3. **Local IoT server**  Python/Flask server with three-layer architecture (Communication  Processing  Storage), two-layer deduplication, auto-discovery of network topology, thread-safe concurrent CSV writes, and REST API for configuration.

4. **End-to-end IoT pipeline**  Complete data path: outdoor sensor  LoRa relay  STM32 gateway  UART  ESP32  MQTT  Flask server  local web dashboard  ThingsBoard cloud.

---

## 6. Repository Structure

```
main/
 README.md                          This file (project overview, team, results)

 Full_local/                        Local IoT server (Python + Flask + MQTT + CSV)
    README.md                      Server architecture, REST API, MQTT protocol, configuration

 WSN_LoRa_Firmware/                 All embedded firmware
    README.md                      Firmware overview: protocol, timing, hardware BOM, toolchains
    WSN_sensor_node/               STM32 sensor node firmware
       README.md                  Sensor hardware, TDMA, registration, power management
    WSN_relay_node/                STM32 relay node firmware
       README.md                  Relay state machine, aggregation, wakeup offsets
    WSN_gateway_node/              STM32 gateway (LoRa side) firmware
       README.md                  Event loop, UART output format, GW_REG_ACK broadcast
    WSN_gateway_forward/           ESP32 UART-to-MQTT bridge firmware
        README.md                  WiFi/MQTT setup, topic mapping, UART protocol

 Backend_sv/                        Optional cloud backend forwarding script
     TEST_GUIDE.md
```

### README Guide

| File | What to read it for |
|------|---------------------|
| This file | Project background, team, technical context, overall results |
| [Full_local/README.md](Full_local/README.md) | Setting up and operating the local server; REST API reference; MQTT topic definitions; CSV database schema |
| [WSN_LoRa_Firmware/README.md](WSN_LoRa_Firmware/README.md) | Complete protocol reference (function codes, frame formats, timing); hardware BOM; deployment order |
| [WSN_sensor_node/README.md](WSN_LoRa_Firmware/WSN_sensor_node/README.md) | Sensor node internals: TDMA schedule, DHT22/ADC integration, registration flow, RTC STOP mode |
| [WSN_relay_node/README.md](WSN_LoRa_Firmware/WSN_relay_node/README.md) | Relay node internals: three-task cycle, slot assignment, inter-relay wakeup staggering |
| [WSN_gateway_node/README.md](WSN_LoRa_Firmware/WSN_gateway_node/README.md) | STM32 gateway: event-driven main loop, UART output/input format, GW_REG_ACK construction |
| [WSN_gateway_forward/README.md](WSN_LoRa_Firmware/WSN_gateway_forward/README.md) | ESP32 bridge: WiFi/MQTT configuration, UART-to-topic mapping, topic-to-UART forwarding |

---

## 7. Quick Start

### Prerequisites

- Python 3.8+, mosquitto MQTT broker (local server)
- STM32CubeIDE (for STM32 firmware)
- PlatformIO with espressif32 platform (for ESP32 firmware)
- ST-Link v2 programmer (for STM32 flashing)

### Local Server

```bash
cd Full_local
pip install -r requirements.txt
# Start mosquitto broker (or use system service)
python app.py
# Dashboard: http://localhost:5000
```

### Firmware (See WSN_LoRa_Firmware/README.md for full deployment order)

1. Configure WiFi/MQTT credentials in `WSN_gateway_forward/src/main.cpp`, then build and flash with PlatformIO.
2. Open `WSN_gateway_node` in STM32CubeIDE, build and flash to the STM32 gateway board.
3. Connect the two gateway boards (STM32 TX  ESP32 RX, STM32 RX  ESP32 TX, shared GND).
4. Flash relay node and sensor node firmware to respective boards.
5. Power on in order: gateway  relays  sensors.
6. From the server dashboard, send an initial configuration to trigger relay registration.

---

## 8. Documentation

- **Full technical report** (142 pages): `Report.pdf`  covers hardware design, link budget analysis, protocol design, software architecture, all experimental results, and project evaluation.
- **Presentation slides**: `Slide.pdf`  summary of architecture decisions and demo results.
