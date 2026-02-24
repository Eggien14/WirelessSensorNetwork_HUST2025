/**
 * ESP32 Gateway - UART to MQTT Bridge
 * Project: WSN25 - IoT Irrigation System
 * Date: 2026-01-21 (Updated: Hex ID Format)
 * 
 * ID FORMAT: Hex strings "0x01", "0xab", "0xFF" (KHÔNG phải số thập phân)
 * 
 * Chức năng:
 * 1. UART nhận "ADV,0x01,0x15,0x23,..." → MQTT publish topic "Advertise" với payload "0x01,0x15,0x23,..."
 * 2. UART nhận "DATA,0x01,0x01,28.5,65.2,45.3,..." → MQTT publish topic "Data" với payload "0x01,0x01,28.5,65.2,45.3,..."
 * 3. MQTT nhận topic "Cycle" với message "120,0x01,60,0x15,90,..." → UART gửi "120,0x01,60,0x15,90,..." (KHÔNG có prefix)
 * 
 * LƯU Ý: ESP32 chỉ FORWARD messages, KHÔNG convert ID format. IDs đã là hex strings từ relay nodes.
 */

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>

// ==================== WiFi Configuration ====================
const char* WIFI_SSID = ".";        // Thay bằng tên WiFi của bạn
const char* WIFI_PASSWORD = "........"; // Thay bằng mật khẩu WiFi

// ==================== MQTT Configuration ====================
const char* MQTT_BROKER = "172.20.149.45";  // Thay bằng IP máy chạy Local Server
const int MQTT_PORT = 1883;
const char* MQTT_CLIENT_ID = "ESP32_Gateway";

// MQTT Topics
const char* TOPIC_ADVERTISE = "Advertise";
const char* TOPIC_DATA = "Data";
const char* TOPIC_CYCLE = "Cycle";

// ==================== UART Configuration ====================
#define UART_RX_PIN 16  // GPIO16 - RX2
#define UART_TX_PIN 17  // GPIO17 - TX2
#define UART_BAUD 115200

// ==================== Global Objects ====================
WiFiClient espClient;
PubSubClient mqttClient(espClient);

// Buffer cho UART
String uartBuffer = "";

// ==================== Function Declarations ====================
void setupWiFi();
void reconnectMQTT();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void handleUARTMessage(String message);
void forwardToMQTT(String command, String payload);

// ==================== Setup ====================
void setup() {
  // Khởi tạo Serial cho debug
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\n========================================");
  Serial.println("ESP32 Gateway - WSN25 IoT System");
  Serial.println("========================================");
  
  // Khởi tạo UART (Serial2) để giao tiếp với relay nodes
  Serial2.begin(UART_BAUD, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);
  Serial.println("[UART] Initialized on Serial2");
  Serial.printf("[UART] RX=%d, TX=%d, Baud=%d\n", UART_RX_PIN, UART_TX_PIN, UART_BAUD);
  
  // Kết nối WiFi
  setupWiFi();
  
  // Cấu hình MQTT
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  
  Serial.println("[SETUP] Completed!");
  Serial.println("========================================\n");
}

// ==================== Main Loop ====================
void loop() {
  // Duy trì kết nối MQTT
  if (!mqttClient.connected()) {
    reconnectMQTT();
  }
  mqttClient.loop();
  
  // Đọc dữ liệu từ UART
  while (Serial2.available()) {
    char c = Serial2.read();
    
    if (c == '\n' || c == '\r') {
      // Kết thúc message
      if (uartBuffer.length() > 0) {
        Serial.printf("[UART ←] %s\n", uartBuffer.c_str());
        
        // Echo lại tin nhắn qua UART
//        Serial2.println(uartBuffer);
        Serial.printf("[UART →]: %s\r\n", uartBuffer.c_str());
        
        handleUARTMessage(uartBuffer);
        uartBuffer = "";
      }
    } else {
      uartBuffer += c;
    }
  }
}

// ==================== WiFi Setup ====================
void setupWiFi() {
  Serial.print("[WiFi] Connecting to ");
  Serial.println(WIFI_SSID);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WiFi] ✓ Connected!");
    Serial.print("[WiFi] IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n[WiFi] ✗ Connection failed!");
    Serial.println("[WiFi] Restarting ESP32...");
    delay(3000);
    ESP.restart();
  }
}

// ==================== MQTT Reconnect ====================
void reconnectMQTT() {
  static unsigned long lastAttempt = 0;
  unsigned long now = millis();
  
  // Thử reconnect mỗi 5 giây
  if (now - lastAttempt < 5000) {
    return;
  }
  lastAttempt = now;
  
  Serial.print("[MQTT] Connecting to broker ");
  Serial.print(MQTT_BROKER);
  Serial.print(":");
  Serial.print(MQTT_PORT);
  Serial.print("... ");
  
  if (mqttClient.connect(MQTT_CLIENT_ID)) {
    Serial.println("✓ Connected!");
    
    // Subscribe topic Cycle
    mqttClient.subscribe(TOPIC_CYCLE);
    Serial.printf("[MQTT] ✓ Subscribed to '%s'\n", TOPIC_CYCLE);
  } else {
    Serial.print("✗ Failed, rc=");
    Serial.println(mqttClient.state());
  }
}

// ==================== MQTT Callback ====================
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  // Chuyển payload thành String
  String message = "";
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  
  Serial.printf("[MQTT →] Topic: %s, Message: %s\n", topic, message.c_str());
  
  // Xử lý topic "Cycle" → Forward xuống UART
  if (strcmp(topic, TOPIC_CYCLE) == 0) {
    // Format: "120,0x01,60,0x15,90,..." (T, ID1, delta_t1, ID2, delta_t2, ...)
    // IDs ở dạng hex string: "0x01", "0xab", "0xFF"
    // Gửi nguyên message xuống UART (KHÔNG thêm prefix "CYCLE,")
    Serial2.println(message);
    Serial.printf("[UART →] %s\n", message.c_str());
  }
}

// ==================== Handle UART Message ====================
void handleUARTMessage(String message) {
  message.trim();
  
  if (message.length() == 0) {
    return;
  }
  
  // Tìm vị trí dấu phẩy đầu tiên để tách command
  int firstComma = message.indexOf(',');
  
  if (firstComma == -1) {
    Serial.println("[UART] ✗ Invalid format (no comma found)");
    return;
  }
  
  // Tách command và payload
  String command = message.substring(0, firstComma);
  String payload = message.substring(firstComma + 1);
  
  command.trim();
  payload.trim();
  
  Serial.printf("[UART] Command: '%s', Payload: '%s'\n", command.c_str(), payload.c_str());
  
  // Forward lên MQTT
  forwardToMQTT(command, payload);
}

// ==================== Forward UART to MQTT ====================
void forwardToMQTT(String command, String payload) {
  const char* topic = nullptr;
  
  // Command "ADV" → Topic "Advertise", payload = "0x01,0x15,0x23,..."
  // Xác định topic dựa vào command 
  // Command "DATA" → Topic "Data", payload = "0x01,0x01,28.5,65.2,45.3,..."
  // IDs luôn ở dạng hex string "0xXX"
  if (command.equalsIgnoreCase("ADV")) {
    topic = TOPIC_ADVERTISE;
  } 
  else if (command.equalsIgnoreCase("DATA")) {
    topic = TOPIC_DATA;
  }
  else {
    Serial.printf("[MQTT] ✗ Unknown command: '%s'\n", command.c_str());
    return;
  }
  
  // Publish lên MQTT
  if (mqttClient.connected()) {
    bool success = mqttClient.publish(topic, payload.c_str());
    
    if (success) {
      Serial.printf("[MQTT ←] ✓ Published to '%s': %s\n", topic, payload.c_str());
    } else {
      Serial.printf("[MQTT ←] ✗ Publish failed to '%s'\n", topic);
    }
  } else {
    Serial.println("[MQTT] ✗ Not connected, cannot publish");
  }
}