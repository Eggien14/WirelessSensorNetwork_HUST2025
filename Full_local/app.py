# -*- coding: utf-8 -*-
"""
Flask Backend Server - Local IoT System
"""

from flask import Flask, jsonify, request, send_from_directory
from flask_cors import CORS
from datetime import datetime
import logging
import os
import json

from config import *
from database_manager import DatabaseManager
from mqtt_handler import MQTTHandler

# Setup logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)

# Khởi tạo Flask app
app = Flask(__name__, static_folder='static', static_url_path='')
CORS(app)

# Khởi tạo Database Manager
db = DatabaseManager(CSV_ADV, CSV_CYCLE, CSV_DATA, CSV_OLD_DATA)

# Khởi tạo MQTT Handler
mqtt = MQTTHandler(MQTT_BROKER, MQTT_PORT, MQTT_KEEPALIVE)

# Biến lưu trạng thái hệ thống
from config import SYSTEM_STATE
import json

# State management
class SystemState:
    def __init__(self):
        self.running = False
        self.selected_relays = []
        self.total_cycle = 120  # Mặc định 120 phút
        self.load_state()
    
    def load_state(self):
        """Load trạng thái từ file"""
        try:
            if os.path.exists(SYSTEM_STATE):
                with open(SYSTEM_STATE, 'r', encoding='utf-8') as f:
                    data = json.load(f)
                    self.running = data.get('running', False)
                    self.selected_relays = data.get('selected_relays', [])
                    self.total_cycle = data.get('total_cycle', 120)
                    logger.info(f"Loaded state: running={self.running}, relays={self.selected_relays}, T={self.total_cycle}")
        except Exception as e:
            logger.error(f"Error loading state: {e}")
    
    def save_state(self):
        """Lưu trạng thái vào file"""
        try:
            os.makedirs(os.path.dirname(SYSTEM_STATE), exist_ok=True)
            with open(SYSTEM_STATE, 'w', encoding='utf-8') as f:
                json.dump({
                    'running': self.running,
                    'selected_relays': self.selected_relays,
                    'total_cycle': self.total_cycle
                }, f, indent=2)
            logger.info(f"Saved state: running={self.running}, T={self.total_cycle}")
        except Exception as e:
            logger.error(f"Error saving state: {e}")
    
    def start(self, relays, total_cycle=120):
        """Bắt đầu hệ thống"""
        self.running = True
        self.selected_relays = relays
        self.total_cycle = total_cycle
        self.save_state()
    
    def stop(self):
        """Dừng hệ thống"""
        self.running = False
        self.save_state()

system_state = SystemState()
thresholds = DEFAULT_THRESHOLDS.copy()


# ==================== MQTT Callbacks ====================

def handle_advertise(payload: str):
    """Xử lý tin nhắn từ topic Advertise
    Format: "ID1,ID2,ID3,..."
    """
    try:
        # LƯU RAW MESSAGE TRƯỚC
        db.save_raw_message('Advertise', payload)
        
        relay_ids = [rid.strip() for rid in payload.split(',') if rid.strip()]
        logger.info(f"📢 Nhận Advertise từ {len(relay_ids)} relay: {relay_ids}")
        
        # Cập nhật vào database
        db.update_relay_advertise(relay_ids)
        
    except Exception as e:
        logger.error(f"✗ Lỗi xử lý Advertise: {e}")


def handle_data(payload: str):
    """Xử lý tin nhắn từ topic Data
    Format: "Relay_ID1,ID1,temp1,humid1,soil1,Relay_ID2,ID2,temp2,humid2,soil2,..."
    """
    try:
        # ==================== CHECK + SAVE MESSAGE (ATOMIC) ====================
        if not db.save_message_if_new('Data', payload):
            logger.warning(f"⚠️ Message ĐÃ XỬ LÝ, BỎ QUA")
            return
        
        logger.info(f"📥 RAW MQTT Data: {payload}")
        parts = [p.strip() for p in payload.split(',')]
        
        # Mỗi sensor có 5 phần: relay_id, sensor_id, temp, humid, soil
        if len(parts) % 5 != 0:
            logger.warning(f"⚠ Dữ liệu Data không hợp lệ (không chia hết cho 5): {payload}")
            logger.warning(f"   Số phần tử: {len(parts)}, Parts: {parts}")
            return
        
        num_sensors = len(parts) // 5
        logger.info(f"📊 Nhận dữ liệu từ {num_sensors} sensor")
        
        # Thu thập TOÀN BỘ sensors trước
        sensors_data = []
        for i in range(num_sensors):
            idx = i * 5
            relay_id = parts[idx]
            sensor_id = parts[idx + 1]
            temp = float(parts[idx + 2])
            humid = float(parts[idx + 3])
            soil = float(parts[idx + 4])
            
            sensors_data.append({
                'relay_id': relay_id,
                'sensor_id': sensor_id,
                'temp': temp,
                'humid': humid,
                'soil': soil
            })
            
            logger.info(f"  ✓ Sensor {sensor_id} (Relay {relay_id}): T={temp}°C, H={humid}%, S={soil}%")
        
        # LƯU TOÀN BỘ sensors MỘT LẦN (ĐỂ TRÁNH RACE CONDITION)
        db.update_multiple_sensors(sensors_data)
        logger.info(f"✅ Đã lưu {len(sensors_data)} sensors vào DATA.csv")
        
        # Cập nhật ADV.csv cho các sensors không phải relay
        for data in sensors_data:
            if data['sensor_id'] != data['relay_id']:
                db.update_sensor_to_relay(data['relay_id'], data['sensor_id'])
        
        # ==================== TÁC VỤ CUỐI CÙNG: LƯU OLD_DATA ====================
        # Lưu timestamp hiện tại
        current_timestamp = datetime.now().strftime('%Y-%m-%d %H:%M:%S')
        
        # Lưu tất cả sensors vào OLD_DATA.csv
        for data in sensors_data:
            db._append_to_old_data({
                'relay_id': data['relay_id'],
                'sensor_id': data['sensor_id'],
                'temp': str(data['temp']),
                'humid': str(data['humid']),
                'soil': str(data['soil']),
                'timestamp': current_timestamp
            })
        logger.info(f"✅ Đã lưu vào OLD_DATA.csv")
        
        # Log tổng số sensors trong DATA.csv sau khi update
        all_data = db.get_all_data()
        logger.info(f"📋 Tổng số sensors trong DATA.csv: {len(all_data)}")
            
    except Exception as e:
        logger.error(f"✗ Lỗi xử lý Data: {e}", exc_info=True)  # Thêm stack trace


# ==================== Flask Routes ====================

@app.route('/')
def index():
    """Trang chủ - Manager Dashboard"""
    return send_from_directory('static', 'index.html')


@app.route('/api/relays', methods=['GET'])
def get_relays():
    """Lấy danh sách tất cả relay và sensor"""
    try:
        relays = db.get_all_relays()
        cycles = db.get_all_cycles()
        
        # Format dữ liệu
        relay_list = []
        for relay_id, sensor_ids in relays.items():
            relay_list.append({
                'relay_id': relay_id,
                'sensor_ids': sensor_ids,
                'delta_t': cycles.get(relay_id, 60)
            })
        
        return jsonify({
            'success': True,
            'relays': relay_list
        })
    except Exception as e:
        logger.error(f"✗ Lỗi API get_relays: {e}")
        return jsonify({'success': False, 'error': str(e)}), 500


@app.route('/api/relay/<relay_id>', methods=['DELETE'])
def delete_relay(relay_id):
    """Xóa relay khỏi hệ thống"""
    try:
        # Không cho xóa khi hệ thống đang chạy
        if system_state.running:
            return jsonify({
                'success': False,
                'error': 'Không thể xóa relay khi hệ thống đang chạy'
            }), 400
        
        # Xóa khỏi database (ADV.csv và CYCLE.csv)
        db.delete_relay(relay_id)
        
        # Xóa khỏi selected_relays nếu đang được chọn
        if relay_id in system_state.selected_relays:
            system_state.selected_relays.remove(relay_id)
            system_state.save_state()
        
        logger.info(f"🗑️ Đã xóa relay {relay_id} khỏi hệ thống")
        
        return jsonify({
            'success': True,
            'message': f'Đã xóa relay {relay_id}'
        })
    except Exception as e:
        logger.error(f"✗ Lỗi API delete_relay: {e}")
        return jsonify({'success': False, 'error': str(e)}), 500


@app.route('/api/data', methods=['GET'])
def get_data():
    """Lấy tất cả dữ liệu sensor"""
    try:
        data = db.get_all_data()
        return jsonify({
            'success': True,
            'data': data
        })
    except Exception as e:
        logger.error(f"✗ Lỗi API get_data: {e}")
        return jsonify({'success': False, 'error': str(e)}), 500


@app.route('/api/sensor/<relay_id>/<sensor_id>', methods=['GET'])
def get_sensor_detail(relay_id, sensor_id):
    """Lấy chi tiết một sensor"""
    try:
        time_range = request.args.get('time_range', 'all')
        history = db.get_sensor_history(relay_id, sensor_id, time_range)
        relays = db.get_all_relays()
        
        # Kiểm tra xem có phải relay không
        is_relay = (relay_id == sensor_id)
        managed_sensors = relays.get(relay_id, []) if is_relay else None
        
        return jsonify({
            'success': True,
            'relay_id': relay_id,
            'sensor_id': sensor_id,
            'is_relay': is_relay,
            'managed_sensors': managed_sensors,
            'history': history
        })
    except Exception as e:
        logger.error(f"✗ Lỗi API get_sensor_detail: {e}")
        return jsonify({'success': False, 'error': str(e)}), 500


@app.route('/api/update_cycle', methods=['POST'])
def update_cycle():
    """Cập nhật cấu hình cycle cho relay"""
    try:
        data = request.json
        relay_id = data.get('relay_id')
        delta_t = int(data.get('delta_t', 60))
        
        db.update_cycle(relay_id, delta_t)
        
        logger.info(f"⚙ Cập nhật cycle cho Relay {relay_id}: {delta_t} phút")
        
        return jsonify({
            'success': True,
            'message': f'Đã cập nhật cycle cho Relay {relay_id}'
        })
    except Exception as e:
        logger.error(f"✗ Lỗi API update_cycle: {e}")
        return jsonify({'success': False, 'error': str(e)}), 500


@app.route('/api/start', methods=['POST'])
def start_system():
    """Bắt đầu hệ thống - gửi tin nhắn Cycle"""
    try:
        data = request.json
        relays = data.get('selected_relays', [])
        total_cycle = data.get('total_cycle', 120)  # Mặc định 120 phút nếu không có
        
        if not relays:
            return jsonify({
                'success': False,
                'error': 'Vui lòng chọn ít nhất một relay'
            }), 400
        
        # Validate total_cycle
        if total_cycle < 1:
            return jsonify({
                'success': False,
                'error': 'Chu kỳ tổng (T) phải lớn hơn 0'
            }), 400
        
        # Tạo message Cycle với T ở đầu
        cycle_message = db.get_cycle_message(relays, total_cycle)
        
        # Gửi qua MQTT
        mqtt.publish_cycle(cycle_message)
        
        # Cập nhật state (lưu cả total_cycle)
        system_state.start(relays, total_cycle)
        
        logger.info(f"🚀 Hệ thống đã START với {len(relays)} relay, T={total_cycle} phút")
        
        return jsonify({
            'success': True,
            'message': f'Đã khởi động hệ thống với {len(relays)} relay, T={total_cycle} phút',
            'running': True,
            'selected_relays': relays,
            'total_cycle': total_cycle
        })
    except Exception as e:
        logger.error(f"✗ Lỗi API start: {e}")
        return jsonify({'success': False, 'error': str(e)}), 500


@app.route('/api/stop', methods=['POST'])
def stop_system():
    """Dừng hệ thống"""
    try:
        system_state.stop()
        logger.info("⏹ Hệ thống đã STOP")
        
        return jsonify({
            'success': True,
            'message': 'Đã dừng hệ thống',
            'running': False
        })
    except Exception as e:
        logger.error(f"✗ Lỗi API stop: {e}")
        return jsonify({'success': False, 'error': str(e)}), 500


@app.route('/api/status', methods=['GET'])
def get_status():
    """Lấy trạng thái hệ thống"""
    return jsonify({
        'success': True,
        'running': system_state.running,
        'selected_relays': system_state.selected_relays,
        'total_cycle': system_state.total_cycle
    })


@app.route('/api/thresholds', methods=['GET', 'POST'])
def handle_thresholds():
    """Lấy/Cập nhật ngưỡng cảnh báo"""
    global thresholds
    
    if request.method == 'GET':
        return jsonify({
            'success': True,
            'thresholds': thresholds
        })
    else:  # POST
        try:
            data = request.json
            thresholds.update(data)
            logger.info(f"⚙ Cập nhật ngưỡng cảnh báo: {thresholds}")
            return jsonify({
                'success': True,
                'message': 'Đã cập nhật ngưỡng cảnh báo',
                'thresholds': thresholds
            })
        except Exception as e:
            logger.error(f"✗ Lỗi API update_thresholds: {e}")
            return jsonify({'success': False, 'error': str(e)}), 500


# ==================== Khởi động ====================

def initialize_mqtt():
    """Khởi tạo kết nối MQTT"""
    try:
        mqtt.connect()
        mqtt.subscribe_advertise(handle_advertise)
        mqtt.subscribe_data(handle_data)
        logger.info("✓ MQTT đã sẵn sàng")
    except Exception as e:
        logger.error(f"✗ Không thể khởi động MQTT: {e}")
        logger.warning("⚠ Server sẽ chạy nhưng không có kết nối MQTT")


if __name__ == '__main__':
    logger.info("=" * 60)
    logger.info("🌾 LOCAL IoT SERVER - WSN25")
    logger.info("=" * 60)
    
    # Khởi tạo MQTT CHỈ 1 LẦN (tránh duplicate khi Flask reloader chạy)
    # Kiểm tra biến môi trường WERKZEUG_RUN_MAIN để biết có phải main process không
    import os
    if os.environ.get('WERKZEUG_RUN_MAIN') == 'true' or not FLASK_DEBUG:
        initialize_mqtt()
    
    # Chạy Flask server
    logger.info(f"🌐 Server đang chạy tại http://{FLASK_HOST}:{FLASK_PORT}")
    logger.info("=" * 60)
    
    app.run(host=FLASK_HOST, port=FLASK_PORT, debug=FLASK_DEBUG)
