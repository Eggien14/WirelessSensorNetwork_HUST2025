# -*- coding: utf-8 -*-
"""
MQTT Handler để nhận/gửi tin nhắn với Gateway
"""

import paho.mqtt.client as mqtt
import logging
from typing import Callable, Optional

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)


class MQTTHandler:
    """Xử lý kết nối và giao tiếp MQTT"""
    
    def __init__(self, broker: str, port: int, keepalive: int = 60):
        self.broker = broker
        self.port = port
        self.keepalive = keepalive
        self.client = mqtt.Client()
        
        # Callbacks
        self.on_advertise_callback: Optional[Callable] = None
        self.on_data_callback: Optional[Callable] = None
        
        # Deduplication - Lưu message cuối để tránh duplicate
        self.last_message = {}  # {topic: (payload, timestamp)}
        
        # Setup callbacks
        self.client.on_connect = self._on_connect
        self.client.on_message = self._on_message
        
    def _on_connect(self, client, userdata, flags, rc):
        """Callback khi kết nối MQTT"""
        if rc == 0:
            logger.info(f"✓ Đã kết nối MQTT Broker tại {self.broker}:{self.port}")
        else:
            logger.error(f"✗ Kết nối MQTT thất bại với mã lỗi: {rc}")
    
    def _on_message(self, client, userdata, msg):
        """Callback khi nhận tin nhắn MQTT"""
        import time
        
        topic = msg.topic
        payload = msg.payload.decode('utf-8')
        current_time = time.time()
        
        # DEDUPLICATE: Bỏ qua nếu message giống hệt message vừa nhận (trong 2 giây)
        if topic in self.last_message:
            last_payload, last_time = self.last_message[topic]
            if last_payload == payload and (current_time - last_time) < 2.0:
                logger.warning(f"⚠️ DUPLICATE message bị bỏ qua từ '{topic}': {payload}")
                return
        
        # Lưu message hiện tại
        self.last_message[topic] = (payload, current_time)
        
        logger.info(f"📨 Nhận tin nhắn từ topic '{topic}': {payload}")
        
        if topic == "Advertise" and self.on_advertise_callback:
            self.on_advertise_callback(payload)
        elif topic == "Data" and self.on_data_callback:
            self.on_data_callback(payload)
    
    def connect(self):
        """Kết nối tới MQTT Broker"""
        try:
            self.client.connect(self.broker, self.port, self.keepalive)
            self.client.loop_start()
            logger.info("🔄 MQTT loop đã bắt đầu")
        except Exception as e:
            logger.error(f"✗ Lỗi kết nối MQTT: {e}")
            raise
    
    def disconnect(self):
        """Ngắt kết nối MQTT"""
        self.client.loop_stop()
        self.client.disconnect()
        logger.info("🔌 Đã ngắt kết nối MQTT")
    
    def subscribe_advertise(self, callback: Callable):
        """Đăng ký nhận topic Advertise"""
        self.on_advertise_callback = callback
        self.client.subscribe("Advertise")
        logger.info("📥 Đã đăng ký topic 'Advertise'")
    
    def subscribe_data(self, callback: Callable):
        """Đăng ký nhận topic Data"""
        self.on_data_callback = callback
        self.client.subscribe("Data")
        logger.info("📥 Đã đăng ký topic 'Data'")
    
    def publish_cycle(self, message: str):
        """Gửi tin nhắn tới topic Cycle"""
        self.client.publish("Cycle", message)
        logger.info(f"📤 Đã gửi tin nhắn tới topic 'Cycle': {message}")
