# -*- coding: utf-8 -*-
"""
Quản lý CSV Database cho hệ thống IoT
"""

import csv
import os
import threading
from datetime import datetime
from typing import List, Dict, Optional


class DatabaseManager:
    """Quản lý 4 file CSV: ADV, CYCLE, DATA, OLD_DATA + MSG (raw MQTT)"""
    
    def __init__(self, adv_file: str, cycle_file: str, data_file: str, old_data_file: str):
        self.adv_file = adv_file
        self.cycle_file = cycle_file
        self.data_file = data_file
        self.old_data_file = old_data_file
        self.msg_file = os.path.join(os.path.dirname(data_file), 'MSG.csv')  # RAW MQTT messages
        
        # CRITICAL: Thread locks để bảo vệ file operations khỏi race conditions
        self._adv_lock = threading.Lock()
        self._cycle_lock = threading.Lock()
        self._data_lock = threading.Lock()
        self._old_data_lock = threading.Lock()
        self._msg_lock = threading.Lock()  # Lock cho MSG.csv
        
        self._ensure_database_exists()
    
    def _ensure_database_exists(self):
        """Tạo thư mục và file CSV nếu chưa có"""
        os.makedirs(os.path.dirname(self.adv_file), exist_ok=True)
        
        # ADV.csv: relay_id, sensor_ids (comma separated)
        if not os.path.exists(self.adv_file):
            with open(self.adv_file, 'w', newline='', encoding='utf-8') as f:
                writer = csv.writer(f)
                writer.writerow(['relay_id', 'sensor_ids'])
        
        # CYCLE.csv: relay_id, delta_t
        if not os.path.exists(self.cycle_file):
            with open(self.cycle_file, 'w', newline='', encoding='utf-8') as f:
                writer = csv.writer(f)
                writer.writerow(['relay_id', 'delta_t'])
        
        # DATA.csv: relay_id, sensor_id, temp, humid, soil, timestamp
        if not os.path.exists(self.data_file):
            with open(self.data_file, 'w', newline='', encoding='utf-8') as f:
                writer = csv.writer(f)
                writer.writerow(['relay_id', 'sensor_id', 'temp', 'humid', 'soil', 'timestamp'])
        
        # OLD_DATA.csv: lưu lịch sử dữ liệu
        if not os.path.exists(self.old_data_file):
            with open(self.old_data_file, 'w', newline='', encoding='utf-8') as f:
                writer = csv.writer(f)
                writer.writerow(['relay_id', 'sensor_id', 'temp', 'humid', 'soil', 'timestamp'])
        
        # MSG.csv: LƯU RAW MQTT MESSAGES
        if not os.path.exists(self.msg_file):
            with open(self.msg_file, 'w', newline='', encoding='utf-8') as f:
                writer = csv.writer(f)
                writer.writerow(['topic', 'message', 'timestamp'])
    
    # ==================== MSG.csv - RAW MQTT MESSAGES ====================
    
    def save_raw_message(self, topic: str, message: str):
        """LƯU RAW MQTT MESSAGE vào MSG.csv TRƯỚC KHI XỬ LÝ
        Args:
            topic: MQTT topic (Advertise, Data, Cycle)
            message: Raw MQTT message
        """
        timestamp = datetime.now().strftime('%Y-%m-%d %H:%M:%S')
        
        with self._msg_lock:
            with open(self.msg_file, 'a', newline='', encoding='utf-8') as f:
                writer = csv.writer(f)
                writer.writerow([topic, message, timestamp])
    
    def get_last_message_timestamp(self) -> str:
        """Lấy timestamp của message cuối cùng trong MSG.csv"""
        try:
            with self._msg_lock:
                if not os.path.exists(self.msg_file):
                    return ""
                with open(self.msg_file, 'r', encoding='utf-8') as f:
                    reader = csv.DictReader(f)
                    rows = list(reader)
                    if rows:
                        return rows[-1].get('timestamp', '')
                    return ""
        except:
            return ""
    
    def is_message_processed(self, topic: str, message: str) -> bool:
        """Kiểm tra xem message đã được xử lý chưa (check MSG.csv)"""
        try:
            with self._msg_lock:
                if not os.path.exists(self.msg_file):
                    return False
                with open(self.msg_file, 'r', encoding='utf-8') as f:
                    reader = csv.DictReader(f)
                    for row in reader:
                        if row.get('topic') == topic and row.get('message') == message:
                            return True  # ĐÃ XỬ LÝ RỒI
                    return False  # CHƯA XỬ LÝ
        except:
            return False
    
    def save_message_if_new(self, topic: str, message: str) -> bool:
        """Lưu message vào MSG.csv
        CHỈ check duplicate nếu CÙNG timestamp (trong vòng 1 giây)
        Returns: True nếu message MỚI (đã save), False nếu DUPLICATE cùng giây
        """
        timestamp = datetime.now().strftime('%Y-%m-%d %H:%M:%S')
        
        with self._msg_lock:
            # Đọc vài message cuối để check duplicate CÙNG TIMESTAMP
            last_5 = []
            if os.path.exists(self.msg_file):
                try:
                    with open(self.msg_file, 'r', encoding='utf-8') as f:
                        reader = csv.DictReader(f)
                        all_rows = list(reader)
                        last_5 = all_rows[-5:] if len(all_rows) > 5 else all_rows
                except:
                    last_5 = []
            
            # CHỈ skip nếu CÙNG topic + message + timestamp (duplicate thật sự)
            for row in last_5:
                if (row.get('topic') == topic and 
                    row.get('message') == message and
                    row.get('timestamp') == timestamp):
                    return False  # DUPLICATE CÙNG GIÂY - BỎ QUA
            
            # LƯU MESSAGE
            with open(self.msg_file, 'a', newline='', encoding='utf-8') as f:
                writer = csv.writer(f)
                writer.writerow([topic, message, timestamp])
            return True  # MESSAGE MỚI
    
    # ==================== ADV.csv Management ====================
    
    def update_relay_advertise(self, relay_ids: List[str]):
        """Cập nhật danh sách relay từ topic Advertise
        KHÔNG tạo duplicate - mỗi relay_id chỉ xuất hiện 1 lần
        """
        with self._adv_lock:  # LOCK để tránh race condition
            # Đọc tất cả relay hiện tại
            existing_data = {}
            if os.path.exists(self.adv_file):
                with open(self.adv_file, 'r', encoding='utf-8') as f:
                    reader = csv.DictReader(f)
                    for row in reader:
                        # Chỉ lưu relay_id DUY NHẤT (không duplicate)
                        if row['relay_id'] and row['relay_id'] not in existing_data:
                            existing_data[row['relay_id']] = row['sensor_ids']
            
            # Thêm relay mới (nếu chưa có)
            for relay_id in relay_ids:
                if relay_id and relay_id not in existing_data:
                    existing_data[relay_id] = ''
            
            # Ghi lại toàn bộ file (KHÔNG append, mà overwrite)
            with open(self.adv_file, 'w', newline='', encoding='utf-8') as f:
                writer = csv.writer(f)
                writer.writerow(['relay_id', 'sensor_ids'])
                for relay_id, sensor_ids in sorted(existing_data.items()):
                    writer.writerow([relay_id, sensor_ids])
    
    def update_sensor_to_relay(self, relay_id: str, sensor_id: str):
        """Cập nhật sensor vào relay tương ứng
        Logic:
        1. Kiểm tra sensor đã tồn tại trong ADV.csv chưa
        2. Nếu chưa có: Thêm vào relay
        3. Nếu đã có và đúng relay: KHÔNG LÀM GÌ
        4. Nếu đã có nhưng sai relay: Xóa khỏi relay cũ, thêm vào relay mới
        """
        with self._adv_lock:  # LOCK để tránh race condition
            # Đọc và loại bỏ duplicate relay_id
            relay_data = {}
            if os.path.exists(self.adv_file):
                with open(self.adv_file, 'r', encoding='utf-8') as f:
                    reader = csv.DictReader(f)
                    for row in reader:
                        rid = row['relay_id']
                        if rid and rid not in relay_data:  # Loại duplicate
                            relay_data[rid] = [s.strip() for s in row['sensor_ids'].split(',') if s.strip()]
            
            # Đảm bảo relay tồn tại
            if relay_id not in relay_data:
                relay_data[relay_id] = []
            
            # Kiểm tra sensor đã ở đúng relay chưa
            if sensor_id in relay_data[relay_id]:
                # Đã đúng -> KHÔNG LÀM GÌ
                return
            
            # Xóa sensor khỏi tất cả relay khác (nếu có)
            for rid, sensors in relay_data.items():
                if sensor_id in sensors and rid != relay_id:
                    sensors.remove(sensor_id)
            
            # Thêm sensor vào relay đích (nếu không phải chính nó)
            if sensor_id != relay_id and sensor_id not in relay_data[relay_id]:
                relay_data[relay_id].append(sensor_id)
            
            # Ghi lại file
            with open(self.adv_file, 'w', newline='', encoding='utf-8') as f:
                writer = csv.writer(f)
                writer.writerow(['relay_id', 'sensor_ids'])
                for rid, sensors in sorted(relay_data.items()):
                    writer.writerow([rid, ','.join(sensors)])
    
    def get_all_relays(self) -> Dict[str, List[str]]:
        """Lấy tất cả relay và sensor của chúng
        Returns: {relay_id: [sensor_ids]}
        Tự động loại bỏ duplicate relay_id
        """
        relays = {}
        if os.path.exists(self.adv_file):
            with open(self.adv_file, 'r', encoding='utf-8') as f:
                reader = csv.DictReader(f)
                for row in reader:
                    rid = row['relay_id']
                    # Chỉ lưu relay_id DUY NHẤT (không duplicate)
                    if rid and rid not in relays:
                        sensor_ids = [s.strip() for s in row['sensor_ids'].split(',') if s.strip()]
                        relays[rid] = sensor_ids
        return relays
    
    # ==================== CYCLE.csv Management ====================
    
    def save_raw_message(self, topic: str, message: str):
        """LƯU RAW MQTT MESSAGE vào MSG.csv TRƯỚC KHI XỬ LÝ
        Args:
            topic: MQTT topic (Advertise, Data, Cycle)
            message: Raw MQTT message
        """
        timestamp = datetime.now().strftime('%Y-%m-%d %H:%M:%S')
        
        with self._msg_lock:
            with open(self.msg_file, 'a', newline='', encoding='utf-8') as f:
                writer = csv.writer(f)
                writer.writerow([topic, message, timestamp])
    
    # ==================== CYCLE.csv Management ====================
    
    def update_cycle(self, relay_id: str, delta_t: int):
        """Cập nhật hoặc thêm cấu hình cycle cho relay"""
        with self._cycle_lock:  # LOCK để tránh race condition
            rows = []
            found = False
            
            # Đọc tất cả và cập nhật nếu có
            if os.path.exists(self.cycle_file):
                with open(self.cycle_file, 'r', encoding='utf-8') as f:
                    reader = csv.DictReader(f)
                    for row in reader:
                        if row['relay_id'] == relay_id:
                            row['delta_t'] = str(delta_t)
                            found = True
                        rows.append(row)
            
            # Nếu chưa có thì thêm mới
            if not found:
                rows.append({'relay_id': relay_id, 'delta_t': str(delta_t)})
            
            # Ghi lại file
            with open(self.cycle_file, 'w', newline='', encoding='utf-8') as f:
                writer = csv.DictWriter(f, fieldnames=['relay_id', 'delta_t'])
                writer.writeheader()
                writer.writerows(rows)
    
    def get_all_cycles(self) -> Dict[str, int]:
        """Lấy tất cả cấu hình cycle
        Returns: {relay_id: delta_t}
        """
        cycles = {}
        with open(self.cycle_file, 'r', encoding='utf-8') as f:
            reader = csv.DictReader(f)
            for row in reader:
                cycles[row['relay_id']] = int(row['delta_t'])
        return cycles
    
    def delete_relay(self, relay_id: str):
        """Xóa relay hoàn toàn khỏi hệ thống (ADV.csv và CYCLE.csv)
        Args:
            relay_id: ID của relay cần xóa
        """
        # Xóa khỏi ADV.csv
        with self._adv_lock:
            if os.path.exists(self.adv_file):
                rows = []
                with open(self.adv_file, 'r', encoding='utf-8') as f:
                    reader = csv.DictReader(f)
                    rows = [row for row in reader if row['relay_id'] != relay_id]
                
                with open(self.adv_file, 'w', newline='', encoding='utf-8') as f:
                    writer = csv.DictWriter(f, fieldnames=['relay_id', 'sensor_ids'])
                    writer.writeheader()
                    writer.writerows(rows)
        
        # Xóa khỏi CYCLE.csv
        with self._cycle_lock:
            if os.path.exists(self.cycle_file):
                rows = []
                with open(self.cycle_file, 'r', encoding='utf-8') as f:
                    reader = csv.DictReader(f)
                    rows = [row for row in reader if row['relay_id'] != relay_id]
                
                with open(self.cycle_file, 'w', newline='', encoding='utf-8') as f:
                    writer = csv.DictWriter(f, fieldnames=['relay_id', 'delta_t'])
                    writer.writeheader()
                    writer.writerows(rows)
    
    def get_cycle_message(self, relay_ids: List[str], total_cycle: int = 120) -> str:
        """Tạo message cho topic Cycle
        Format: "T,ID1,delta_t1,ID2,delta_t2,..."
        Args:
            relay_ids: Danh sách relay IDs
            total_cycle: Chu kỳ tổng T (phút) - áp dụng cho tất cả relay
        """
        cycles = self.get_all_cycles()
        parts = [str(total_cycle)]  # Thêm T ở đầu
        for relay_id in relay_ids:
            delta_t = cycles.get(relay_id, 60)  # Mặc định 60 phút nếu chưa cấu hình
            parts.extend([relay_id, str(delta_t)])
        return ','.join(parts)
    
    # ==================== DATA.csv Management ====================
    
    def update_sensor_data(self, relay_id: str, sensor_id: str, temp: float, 
                          humid: float, soil: float):
        """Cập nhật dữ liệu sensor mới (single sensor)
        LƯU Ý: Hàm này ít dùng, chủ yếu dùng update_multiple_sensors()
        """
        timestamp = datetime.now().strftime('%Y-%m-%d %H:%M:%S')
        
        # Tạo dữ liệu mới
        new_data = {
            'relay_id': relay_id,
            'sensor_id': sensor_id,
            'temp': str(temp),
            'humid': str(humid),
            'soil': str(soil),
            'timestamp': timestamp
        }
        
        # LƯU DỮ LIỆU MỚI vào OLD_DATA.csv (history log)
        self._append_to_old_data(new_data)
        
        with self._data_lock:  # LOCK để tránh race condition khi nhiều sensors update đồng thời
            # Đọc tất cả dữ liệu hiện tại
            rows = []
            updated = False
            
            if os.path.exists(self.data_file):
                with open(self.data_file, 'r', encoding='utf-8') as f:
                    reader = csv.DictReader(f)
                    for row in reader:
                        if row['sensor_id'] == sensor_id:  # KEY = sensor_id (độc nhất)
                            # Ghi đè dữ liệu mới
                            row = new_data.copy()
                            updated = True
                        rows.append(row)
            
            # Nếu chưa có thì thêm mới
            if not updated:
                rows.append(new_data)
            
            # Sắp xếp lại theo thứ tự: relay và các sensor của nó
            rows_sorted = self._sort_data_by_relay(rows)
            
            # Ghi lại file
            with open(self.data_file, 'w', newline='', encoding='utf-8') as f:
                writer = csv.DictWriter(f, fieldnames=['relay_id', 'sensor_id', 'temp', 'humid', 'soil', 'timestamp'])
                writer.writeheader()
                writer.writerows(rows_sorted)
    
    def update_multiple_sensors(self, sensors_data: List[Dict]):
        """Cập nhật nhiều sensors cùng lúc (Từ MQTT message)
        KEY = sensor_id (độc nhất) - Ghi đè nếu có, thêm mới nếu chưa
        KHÔNG tự động lưu OLD_DATA - sẽ được gọi riêng từ handle_data()
        Args:
            sensors_data: List[{'relay_id', 'sensor_id', 'temp', 'humid', 'soil'}]
        """
        timestamp = datetime.now().strftime('%Y-%m-%d %H:%M:%S')
        
        with self._data_lock:
            # Đọc dữ liệu hiện tại vào DICT với KEY = sensor_id (độc nhất)
            existing_rows = {}
            if os.path.exists(self.data_file):
                with open(self.data_file, 'r', encoding='utf-8') as f:
                    reader = csv.DictReader(f)
                    for row in reader:
                        key = row['sensor_id']  # KEY = sensor_id (độc nhất)
                        existing_rows[key] = row
            
            # Cập nhật/thêm sensors mới (KHÔNG lưu OLD_DATA ở đây)
            for data in sensors_data:
                key = data['sensor_id']  # KEY = sensor_id
                
                # Tạo dữ liệu mới từ MQTT message
                new_data = {
                    'relay_id': data['relay_id'],
                    'sensor_id': data['sensor_id'],
                    'temp': str(data['temp']),
                    'humid': str(data['humid']),
                    'soil': str(data['soil']),
                    'timestamp': timestamp
                }
                
                # GHI ĐÈ hoặc THÊM MỚI vào DATA.csv (giá trị mới nhất)
                existing_rows[key] = new_data
            
            # Chuyển về list và SẮP XẾP THEO RELAY
            rows = list(existing_rows.values())
            rows_sorted = self._sort_data_by_relay(rows)
            
            # Ghi file MỘT LẦN duy nhất
            with open(self.data_file, 'w', newline='', encoding='utf-8') as f:
                writer = csv.DictWriter(f, fieldnames=['relay_id', 'sensor_id', 'temp', 'humid', 'soil', 'timestamp'])
                writer.writeheader()
                writer.writerows(rows_sorted)

    
    def _sort_data_by_relay(self, rows: List[Dict]) -> List[Dict]:
        """Sắp xếp dữ liệu theo Relay
        Logic: Relay trước (relay_id == sensor_id), sau đó các sensor của relay đó
        ĐẢM BẢO: GIỮ NGUYÊN TẤT CẢ sensors, KHÔNG MẤT DỮ LIỆU
        """
        relays_data = self.get_all_relays()
        sorted_rows = []
        added_sensors = set()  # Track sensors đã thêm (dùng sensor_id vì là KEY độc nhất)
        
        # Bước 1: Sắp xếp theo thứ tự relay trong ADV.csv
        for relay_id, sensor_ids in relays_data.items():
            # Thêm relay trước (relay_id == sensor_id)
            for row in rows:
                if row['sensor_id'] == relay_id and row['sensor_id'] not in added_sensors:
                    sorted_rows.append(row)
                    added_sensors.add(row['sensor_id'])
                    break
            
            # Thêm các sensors của relay này
            for sensor_id in sensor_ids:
                for row in rows:
                    if row['relay_id'] == relay_id and row['sensor_id'] == sensor_id and row['sensor_id'] not in added_sensors:
                        sorted_rows.append(row)
                        added_sensors.add(row['sensor_id'])
                        break
        
        # Bước 2: CRITICAL - Thêm TẤT CẢ sensors còn lại (chưa có trong ADV.csv)
        for row in rows:
            if row['sensor_id'] not in added_sensors:
                sorted_rows.append(row)
                added_sensors.add(row['sensor_id'])
        
        return sorted_rows
    
    def get_all_data(self) -> List[Dict]:
        """Lấy tất cả dữ liệu sensor"""
        data = []
        if os.path.exists(self.data_file):
            with open(self.data_file, 'r', encoding='utf-8') as f:
                reader = csv.DictReader(f)
                for row in reader:
                    data.append(row)
        return data
    
    def _append_to_old_data(self, data_row: Dict):
        """Thêm dữ liệu vào OLD_DATA.csv - CHỈ APPEND đơn giản"""
        with self._old_data_lock:
            try:
                with open(self.old_data_file, 'a', newline='', encoding='utf-8') as f:
                    writer = csv.DictWriter(f, fieldnames=['relay_id', 'sensor_id', 'temp', 'humid', 'soil', 'timestamp'])
                    writer.writerow(data_row)
            except Exception as e:
                print(f"Error appending to OLD_DATA: {e}")
    
    def get_sensor_history(self, relay_id: str, sensor_id: str, time_range: str = 'default') -> List[Dict]:
        """Lấy lịch sử dữ liệu của một sensor từ OLD_DATA.csv
        time_range: 'default' (20 gần nhất), 'minute', 'hour', 'day', 'month'
        AUTO FILTER DUPLICATES khi đọc
        """
        data = []
        seen = set()  # Track duplicates
        now = datetime.now()
        
        # Đọc từ OLD_DATA
        if os.path.exists(self.old_data_file):
            with open(self.old_data_file, 'r', encoding='utf-8') as f:
                reader = csv.DictReader(f)
                for row in reader:
                    if row['relay_id'] == relay_id and row['sensor_id'] == sensor_id:
                        # Filter theo time range (nếu không phải default)
                        if time_range != 'default' and row.get('timestamp'):
                            try:
                                row_time = datetime.strptime(row['timestamp'], '%Y-%m-%d %H:%M:%S')
                                time_diff = now - row_time
                                
                                if time_range == 'minute' and time_diff.total_seconds() > 60:
                                    continue
                                elif time_range == 'hour' and time_diff.total_seconds() > 3600:
                                    continue
                                elif time_range == 'day' and time_diff.total_seconds() > 86400:
                                    continue
                                elif time_range == 'month' and time_diff.total_seconds() > 2592000:
                                    continue
                            except:
                                pass
                        
                        # FILTER DUPLICATES: Tạo key từ tất cả fields
                        key = (row.get('relay_id'), row.get('sensor_id'), 
                               row.get('temp'), row.get('humid'), 
                               row.get('soil'), row.get('timestamp'))
                        if key not in seen:
                            data.append(row)
                            seen.add(key)
        
        # Thêm dữ liệu mới nhất từ DATA.csv
        if os.path.exists(self.data_file):
            with open(self.data_file, 'r', encoding='utf-8') as f:
                reader = csv.DictReader(f)
                for row in reader:
                    if row['relay_id'] == relay_id and row['sensor_id'] == sensor_id:
                        # Check duplicate với DATA.csv
                        key = (row.get('relay_id'), row.get('sensor_id'), 
                               row.get('temp'), row.get('humid'), 
                               row.get('soil'), row.get('timestamp'))
                        if key not in seen:
                            data.append(row)
                            seen.add(key)
        
        # Nếu mode 'default', chỉ lấy 20 data gần nhất
        if time_range == 'default' and len(data) > 20:
            data = data[-20:]  # Lấy 20 phần tử cuối (mới nhất)
        
        return data
