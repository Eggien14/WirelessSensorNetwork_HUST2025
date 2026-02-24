# -*- coding: utf-8 -*-
"""
Cấu hình cho Local IoT Server
"""

# MQTT Configuration
MQTT_BROKER = "localhost"  # Mosquitto chạy trên máy local
MQTT_PORT = 1883
MQTT_KEEPALIVE = 60

# MQTT Topics
TOPIC_ADVERTISE = "Advertise"
TOPIC_CYCLE = "Cycle"
TOPIC_DATA = "Data"

# Flask Server Configuration
FLASK_HOST = "0.0.0.0"  # Cho phép truy cập từ các thiết bị trong mạng LAN
FLASK_PORT = 5000
FLASK_DEBUG = True

# Database (CSV Files)
CSV_ADV = "Database/ADV.csv"
CSV_CYCLE = "Database/CYCLE.csv"
CSV_DATA = "Database/DATA.csv"
CSV_OLD_DATA = "Database/OLD_DATA.csv"
SYSTEM_STATE = "Database/system_state.json"

# Sensor thresholds (Ngưỡng mặc định)
DEFAULT_THRESHOLDS = {
    "temp_min": 15.0,
    "temp_max": 35.0,
    "humid_min": 40.0,
    "humid_max": 80.0,
    "soil_min": 30.0,
    "soil_max": 70.0
}
