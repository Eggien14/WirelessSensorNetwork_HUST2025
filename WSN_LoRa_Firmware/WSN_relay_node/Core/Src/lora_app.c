/*
 * lora_app.c
 *
 *  Created on: Dec 14, 2025
 *      Author: HaiVuQuang
 *      Github: https://github.com/HaiVuQuang
 */

#include <lora_app.h>
#include <stdio.h>
#include <string.h>

extern RTC_HandleTypeDef hrtc;
extern UART_HandleTypeDef huart2;

//Thời gian mỗi chu kỳ
uint16_t TOTAL_CYCLE_SEC = DEFAULT_TOTAL_CYCLE;


// =======================================
// --- Hàm xử lý RTC & chuyển chế độ ---
// =======================================

/*
 * @brief:  Cài đặt LSE RTC Alarm
 * @param:
 * 			seconds: Thời gian (giây) báo thức
 */
void RTC_SetAlarm_In_Seconds(uint32_t seconds) {
	// Đợi đồng bộ hóa RTC
	HAL_RTC_WaitForSynchro(&hrtc);

	while (!(hrtc.Instance->CRL & RTC_CRL_RTOFF));

	// Đọc giá trị bộ đếm hiện tại:
	uint32_t current_counter = (hrtc.Instance->CNTH << 16) | hrtc.Instance->CNTL;
	uint32_t alarm_value = current_counter + seconds;

	//Bật chế độ config flag
	__HAL_RTC_WRITEPROTECTION_DISABLE(&hrtc);
//	if((hrtc.Instance->CRL & RTC_CRL_RTOFF) == (uint32_t)RESET){
//	         // Chờ sẵn sàng (chờ bit RTOFF lên 1s)
//	    }
	hrtc.Instance->CRL |= RTC_CRL_CNF;		//RTC_CRL_CNF: Bit Configuration.
											//Dòng này Dừng việc cập nhật thanh ghi từ bộ đếm để
											//-> cho phép ghi giá trị mới vào ALRH/ALRL
	// Ghi giá trị Alarm
	hrtc.Instance->ALRH = (alarm_value >> 16);		//Lấy 2 byte cao
	hrtc.Instance->ALRL = (alarm_value & 0xFFFF);	//Lấy 2 byte thấp

	//Xóa bit CNF
	hrtc.Instance->CRL &= ~RTC_CRL_CNF;

	while (!(hrtc.Instance->CRL & RTC_CRL_RTOFF));

	// Bật lại bảo vệ ghi
	__HAL_RTC_WRITEPROTECTION_ENABLE(&hrtc);

	// Xóa cờ ngắt cũ
	__HAL_RTC_ALARM_CLEAR_FLAG(&hrtc, RTC_FLAG_ALRAF);

	// Bật ngắt RTC
	__HAL_RTC_ALARM_ENABLE_IT(&hrtc, RTC_IT_ALRA);

	// Bật ngắt EXTI Line 17
	__HAL_RTC_ALARM_EXTI_ENABLE_IT();

	// Cấu hình EXTI bắt sườn lên (Rising Edge)
	__HAL_RTC_ALARM_EXTI_ENABLE_RISING_EDGE();
}


/*
 * @brief:  Vào STOP mode, khôi phục khi có ngắt
 *
 */
void Enter_Stop_Mode(void) {

    // Tắt SysTick để tránh ngắt SysTick đánh thức chip ngay lập tức
    HAL_SuspendTick();

    // Vào chế độ STOP, Regulator ở chế độ Low Power
    HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);

    // ---> STOP <---

    // Bật lại SysTick
    HAL_ResumeTick();

    // Khôi phục lại Clock
    SystemClock_Config_FromStop();

}


//Khôi phục Clock sau khi dậy
void SystemClock_Config_FromStop(void) {
    SystemClock_Config();
}

/*
 * @brief:  Thực hiện bù thời gian tới khi hết timeout một task
 * @param:
 * 			start_tick: Systick khi bắt đầu thực hiện task
 * 			target_duration_ms: Timeout mong muốn (ms)
 *
 */
void Pad_Execution_Time(uint32_t start_tick, uint32_t target_duration_ms) {
    uint32_t elapsed = HAL_GetTick() - start_tick;
    if (target_duration_ms > elapsed) {
        HAL_Delay(target_duration_ms - elapsed);
    }
}

#if (CURRENT_NODE_TYPE == NODE_TYPE_SENSOR)
// ==============================
// --- HÀM PHÍA SENSOR NODE ---
// ==============================

/*
 * @brief:  Thực hiện pha đăng ký với Relay.
 * @param:
 * 			_lora: Con trỏ struct LoRa quản lý
 * 			_rxBuf: Con trỏ buffer nhận
 * 			_rxBufSize: Kích thước buffer nhận
 * 			_rxFlag: Cờ nhận (recieve flag)
 * 			_myID: ID sensor node
 * 			_targetRelayID: ID relay node mục tiêu
 * @return:
 * 			Cycle tổng (total_cycle) và Time Slot (ID khe thời gian) được Relay cấp phát.
 */

uint8_t LoRaApp_Sensor_RegistrationPhase(
		LoRa* _lora, uint8_t* _rxBuf, uint16_t _rxBufSize,
		volatile uint8_t* _rxFlag, uint8_t _myID, uint8_t _targetRelayID) {

	msg_ss_reg_adv_t adv_msg;
	msg_ss_reg_ack_t* ack_msg;
    uint8_t tx_buffer[10];

    printf("\r\n[SENSOR] >>> START REGISTRATION PHASE <<<\r\n");

    // 1. Cấu hình bản tin quảng bá ADV
    adv_msg.func_code = FUNC_CODE_REG_ADV;
    adv_msg.sensor_id = _myID;
    adv_msg.target_relay_id = _targetRelayID;

    // Đóng gói vào buffer
    tx_buffer[0] = adv_msg.func_code;
    tx_buffer[1] = adv_msg.sensor_id;
    tx_buffer[2] = adv_msg.target_relay_id;

	// Vòng lặp gửi và chờ
	while (1) {

//		printf("\r\n[SENSOR] Sending ADV Request to Relay 0x%02X...\r\n", _targetRelayID);

		// Gửi bản tin ADV
		LoRa_setMode(_lora, STNBY_MODE);
		uint8_t tx_result = LoRa_transmit(_lora, tx_buffer, sizeof(msg_ss_reg_adv_t), TRANSMIT_TIMEOUT);

		if (tx_result) {
			printf("[SENSOR] Sending ADV Request to Relay 0x%02X... -> OK \r\n", _targetRelayID);
		} else {
			printf("[SENSOR] ADV transmission FAILED! Check connection.\r\n");
		}

		// Chuyển sang chế độ nhận liên tục để chờ ACK
		LoRa_setMode(_lora, RXCONTIN_MODE);

		uint32_t start_wait = HAL_GetTick();
//		uint8_t received_ack = 0;

		// Chờ trong khoảng thời gian REG_TIMEOUT_MS
		while (HAL_GetTick() - start_wait < REG_TIMEOUT_MS) {

			// Kiểm tra cờ ngắt
			if (*_rxFlag) {
				*_rxFlag = 0; // Xóa cờ ngắt
				memset(_rxBuf, 0, _rxBufSize);

				int len = LoRa_receive(_lora, _rxBuf, _rxBufSize);
				if (len > 0) {
					// Kiểm tra Function Code
					if (_rxBuf[0] == FUNC_CODE_REG_ACK) {

						// Ép kiểu sang msg_ss_reg_ack_t
						ack_msg = (msg_ss_reg_ack_t*)_rxBuf;

						// Kiểm tra ID: Đúng Relay mình gọi và đúng Sensor ID của mình
						if (ack_msg->target_sensor_id == _myID && ack_msg->relay_id == _targetRelayID) {

							// Lấy total_cycle và time slot được cấp phát
							uint8_t assigned_slot = ack_msg->time_slot;
							TOTAL_CYCLE_SEC = ack_msg->total_cycle;

							printf("\r\n[SENSOR] !!! ACK RECEIVED FROM RELAY 0x%02X!!!\r\n", ack_msg->relay_id);
							printf("[SENSOR] Assigned TDMA Slot: %d\r\n", assigned_slot);

							printf("[SENSOR] Syncing Cycle: Sleeping %d seconds to match Relay Start...\r\n", TOTAL_CYCLE_SEC);
							HAL_Delay(10);

							// Cài đặt RTC và ngủ đúng 1 chu kỳ để dậy vào đầu chu kỳ sau
							RTC_SetAlarm_In_Seconds(TOTAL_CYCLE_SEC);
							Enter_Stop_Mode();

							// Khi thức dậy, thoát khỏi hàm và trả về Slot ID
							printf("[SENSOR] Woke up! Registration Complete. Entering Main Loop.\r\n");

							return assigned_slot;
						}
					}
				}
			}
		}
		// Random delay để tránh xung đột
		HAL_Delay(200 + (HAL_GetTick() % 1000));
	}
}


static msg_ss_data_t sensor_latest_data = {0};
//static uint32_t sensor_cycle_count;


/*
 * @brief:  TASK 1: Thực hiện gửi dữ liệu từ Sensor -> Relay (Timeout: SENSOR_TX_WINDOW_MS)
 * @param:
 * 			_lora: Con trỏ struct LoRa quản lý
 * 			_myID: ID sensor node
 * 			_targetRelayID: ID relay node mục tiêu
 * 			_mySlot: TDMA time slot được cấp phát
 *
 */
void LoRaApp_Sensor_Task_SendData(LoRa* _lora, uint8_t _myID, uint8_t _targetRelayID, uint8_t _mySlot) {
    uint32_t start_task = HAL_GetTick();

    // 1. TDMA Delay
    uint32_t tdma_wait = SENSOR_TDMA_BASE_MS + (_mySlot * SENSOR_TDMA_SLOT_MS);

    // Kiểm tra an toàn: TDMA wait không được vượt quá window
    if (tdma_wait > SENSOR_TX_WINDOW_MS - 500) tdma_wait = SENSOR_TX_WINDOW_MS - 500;


    printf("[SENSOR] Wait for TDMA slot to sent DATA: %lu ms\r\n", tdma_wait);
    HAL_Delay(tdma_wait);

    // 2. Đóng gói Data (Latest)
    sensor_latest_data.func_code = FUNC_CODE_SS_DATA;
    sensor_latest_data.sensor_id = _myID;
    sensor_latest_data.target_relay_id = _targetRelayID;

    LoRa_setMode(_lora, STNBY_MODE);

    //Gửi 2 lần
    int result;
    for (int i = 0; i < 2; i++){
    	result = LoRa_transmit(_lora, (uint8_t*)&sensor_latest_data, sizeof(msg_ss_data_t), 300);
    	HAL_Delay(50);
    }

	if (result) {
		printf("[SENSOR] Data Sent: T=%d, H=%d\r\n", sensor_latest_data.temp_val, sensor_latest_data.hum_val);
	} else {
		printf("[SENSOR] Send Data -> FAILED!\r\n");
	}

    //Padding thời gian cho đủ SENSOR_TX_WINDOW_MS
    Pad_Execution_Time(start_task, SENSOR_TX_WINDOW_MS);
}

/*
 * @brief:  TASK 2: Sensor thực hiện đo dữ liệu cảm biến (Timeout: SENSOR_MEASURE_WINDOW_MS)
 * @param:
 * 			_sensorCfg: Con trỏ tới struct quản lý dữ liệu cảm biến
 *
 *
 */

void LoRaApp_Sensor_Task_Measure(Sensor_Config_t* _sensorCfg) {
    uint32_t start_task = HAL_GetTick();
    printf("[SENSOR] Measuring sensor data ... (Timeout: %lu ms)\r\n", SENSOR_MEASURE_WINDOW_MS);

    //Thực hiện chu trình đo
    Sensor_Data_t myData;
    Sensor_ReadAll(_sensorCfg, &myData);

//    //Test với không có cảm biến
//    Sensor_ReadAll_Test(_sensorCfg, &myData);

    //Lưu dữ liệu struct quản lý
    if(myData.status == 0) {
    	sensor_latest_data.temp_val = (int16_t)(myData.temp_c * 10);
    	sensor_latest_data.hum_val  = (uint16_t)(myData.hum_rh * 10);
    	sensor_latest_data.soil_val = myData.soil_percent;
        printf("[SENSOR] Measured: %.1f C, %.1f %%\r\n", myData.temp_c, myData.hum_rh);
    } else {
        printf("[SENSOR] Measure Failed. Keep Old Data.\r\n");
    }

    // Padding thời gian cho đủ SENSOR_MEASURE_WINDOW_MS
    Pad_Execution_Time(start_task, SENSOR_MEASURE_WINDOW_MS);
}

#endif

#if (CURRENT_NODE_TYPE == NODE_TYPE_RELAY)
// ==============================
// --- HÀM PHÍA RELAY NODE ---
// ==============================

//Danh sách sensor node chịu quản lý
static const uint8_t managed_sensors[MANAGED_SENSOR_COUNT] = MANAGED_SENSOR_LIST;
//Struct kiểm soát dữ liệu các sensor chịu quản lý
static Relay_Sensor_Data_Slot_t relay_data_store[MANAGED_SENSOR_COUNT];


/*
 * @brief: 	Kiểm tra xem Sensor ID có nằm trong danh sách quản lý không
 * @param:	sensor_id: ID sensor cần kiểm tra
 * @return: 1 nếu CÓ, 0 nếu KHÔNG
 */

uint8_t IsSensorManaged(uint8_t sensor_id) {
    for (int i = 0; i < MANAGED_SENSOR_COUNT; i++) {
        if (managed_sensors[i] == sensor_id) {
            return 1; // Tìm thấy trong danh sách
        }
    }
    return 0; // Không tìm thấy
}

/*
 * @brief: Cấp phát index slot cho Sensor node dựa theo số thứ tự trong danh sách
 * @param:	sensor_id: ID sensor cần kiểm tra
 * @return: Sensor node index
 */
int GetSensorIndex(uint8_t sensor_id) {
    for (int i = 0; i < MANAGED_SENSOR_COUNT; i++) {
        if (relay_data_store[i].sensor_id == sensor_id) return i;
    }
    return -1;
}


/*
 * @brief: Init/Reset dữ liệu Sensor do Relay quản lý
 */
void LoRaApp_Relay_Init(void) {
    for(int i=0; i<MANAGED_SENSOR_COUNT; i++) {
        // Gán cứng ID từ danh sách quản lý vào Slot để GetSensorIndex tìm thấy
        relay_data_store[i].sensor_id = managed_sensors[i];

        // Reset trạng thái data cũ
        relay_data_store[i].has_data = 0;
        relay_data_store[i].temp = 0;
        relay_data_store[i].hum = 0;
        relay_data_store[i].soil = 0;
    }
}


/*
 * @brief: 	Relay đăng ký với Gateway và chờ cấu hình thời gian
 * 			Hàm này sẽ chặn (Blocking) cho đến khi nhận được Config từ GW
 * @param:
 * 			_lora: Con trỏ struct LoRa quản lý
 * 			_rxBuf: Con trỏ buffer nhận
 * 			_rxBufSize: Kích thước buffer nhận
 * 			_rxFlag: Cờ nhận (recieve flag)
 * 			_myRelayID: ID Relay node
 */
uint8_t LoRaApp_Relay_RegistrationWithGateway(
		LoRa* _lora, uint8_t* _rxBuf, uint16_t _rxBufSize,
		volatile uint8_t* _rxFlag, uint8_t _myRelayID)
{
    msg_rl_reg_adv_t adv_msg;
    uint16_t my_wakeup_offset = 0;
    uint8_t configured = 0;

    printf("\r\n[RELAY] >>> START RELAY REGISTRATION <<<\r\n");

    adv_msg.func_code = FUNC_CODE_RL_REG_ADV;
    adv_msg.relay_id = _myRelayID;
    adv_msg.reserved = 0;

    while(!configured) {
        // Gửi ADV định kỳ
        LoRa_setMode(_lora, STNBY_MODE);
        int result = LoRa_transmit(_lora, (uint8_t*)&adv_msg, sizeof(msg_rl_reg_adv_t), 1000);
        if (result){
        	printf("[RELAY] Sending ADV Request to Gateway...\r\n");
        } else {
        	printf("[RELAY] Sending ADV Request to Gateway -> FAILED...\r\n");
        }
        LoRa_setMode(_lora, RXCONTIN_MODE);


        // Chờ phản hồi Broadcast (Timeout REG_TIMEOUT_MS gửi lại)
        uint32_t start_wait = HAL_GetTick();
        while(HAL_GetTick() - start_wait < REG_TIMEOUT_MS) {
            if(*_rxFlag) {
                *_rxFlag = 0;
                int len = LoRa_receive(_lora, _rxBuf, _rxBufSize);
                if(len > 0 && _rxBuf[0] == FUNC_CODE_GW_REG_ACK) {

                    //Format: [0x07 | Cycle_H | Cycle_L | Count | ... ]
                	uint16_t total_cycle = (_rxBuf[1] << 8) | _rxBuf[2];
                    uint8_t count = _rxBuf[3];
                    uint8_t ptr = 4;	//Data bắt đầu từ byte thứ 4

                    printf("\r\n[RELAY] !!! ACK RECEIVED FROM GATEWAY !!!\r\n");

                    printf("[RELAY] Scanning for My ID and configuration...\r\n");

                    // Quét danh sách để tìm ID của mình
                    for(int i=0; i<count; i++) {
                        uint8_t id = _rxBuf[ptr];
                        uint16_t delta = (_rxBuf[ptr+1] << 8) | _rxBuf[ptr+2];

                        //Nếu có chứa ID bản thân
                        if(id == _myRelayID) {

                            TOTAL_CYCLE_SEC = total_cycle;
                            my_wakeup_offset = delta; // Đơn vị s
                            configured = 1;

                            printf("[RELAY] System configuration set! Cycle: %ds, Wakeup Offset: %ds\r\n", total_cycle, my_wakeup_offset);
                            break;
                        }
                        ptr += 3; // Nhảy sang cặp tiếp theo
                    }
                    if(configured) break;
                }
            }
        }

        if(!configured) {
            HAL_Delay(1000 + (HAL_GetTick() % 1000));
        }
    }

    // Ngủ chờ đến thời điểm Δt (Wakeup Offset) để bắt đầu chu kỳ
    if(my_wakeup_offset > 0) {
        printf("[RELAY] Waiting %d ms to sync start time...\r\n", my_wakeup_offset);
        uint32_t sleep_sec = my_wakeup_offset;

        //Nếu > 1s -> vào STOP mode; < 1s -> HAL_Delay
        if(sleep_sec > 0) {
            RTC_SetAlarm_In_Seconds(sleep_sec);
            Enter_Stop_Mode();
        } else {
            HAL_Delay(my_wakeup_offset);
        }
    }

    printf("[RELAY] Synced! Entering Main Loop.\r\n");
    return 1;
}


/*
 * @brief:  Xử lý dữ liệu nhận được khi Relay đang ở Pha Lắng nghe
 * 			Hàm sẽ phân loại và xử lý bản tin nhận được theo function code
 * @param:
 * 			_lora: Con trỏ struct LoRa quản lý
 * 			_rxBuf: Con trỏ buffer nhận
 * 			_myRelayID: ID Relay node
 * 			_queue: Hàng chờ yêu cầu Đăng ký của Sensor node (xử lý gửi ACK đầu chu kỳ sau)
 *
 */
void LoRaApp_Relay_RxProcessing(
		LoRa* _lora, uint8_t* _rxBuf, uint8_t _myRelayID, Relay_Reg_Queue_t* _queue) {

    uint8_t func_code = _rxBuf[0];

    // --- CASE 1: PHA ĐĂNG KÝ (ADV) ---
    if (func_code == FUNC_CODE_REG_ADV) {
        msg_ss_reg_adv_t* adv_msg = (msg_ss_reg_adv_t*)_rxBuf;

//        printf("[RELAY] Received ADV from Sensor 0x%02X for Relay 0x%02X\r\n",
//                           adv_msg->sensor_id, adv_msg->target_relay_id);

        // Kiểm tra Target Relay ID
        if (adv_msg->target_relay_id != _myRelayID) {
            return;
        }

        // Kiểm tra xem thuộc danh sách quản lý không?
        if (IsSensorManaged(adv_msg->sensor_id)) {

            printf("[RELAY] Received ADV form Managed Sensor: 0x%02X --> ACCEPTED\r\n", adv_msg->sensor_id);

            // Logic thêm vào hàng đợi (Queue logic)
            if (_queue->count < MAX_PENDING_ACK) {
                uint8_t exists = 0;
                for(int i=0; i < _queue->count; i++) {
                    if(_queue->pending_sensors[i] == adv_msg->sensor_id) {
                        exists = 1; break;
                    }
                }

                if (!exists) {
                    _queue->pending_sensors[_queue->count] = adv_msg->sensor_id;
                    _queue->count++;
                    printf("[RELAY] Added Sensor 0x%02X to ACK Queue. Count: %d\r\n", adv_msg->sensor_id, _queue->count);
                }
            }
        }
    }

    // --- CASE 2: PHA BÁO CÁO (HANDLE LƯU DATA) ---
    else if (func_code == FUNC_CODE_SS_DATA) {
        msg_ss_data_t* data_msg = (msg_ss_data_t*)_rxBuf;

        // Kiểm tra Target Relay ID
        if (data_msg->target_relay_id != _myRelayID) {
            return;
        }

        // Kiểm tra xem có thuộc danh sách quản lý?
        if (IsSensorManaged(data_msg->sensor_id)) {
        	printf("[RELAY] Received DATA from 0x%02X: T=%d, H=%d, S=%d\r\n",
        	                   data_msg->sensor_id, data_msg->temp_val, data_msg->hum_val, data_msg->soil_val);

        	int idx = GetSensorIndex(data_msg->sensor_id);

        	// Trả về nếu đã có dữ liệu ở chu kỳ này rồi
        	if (relay_data_store[idx].has_data == 1) return;

        	if (idx >= 0 && idx < MANAGED_SENSOR_COUNT) {
				relay_data_store[idx].temp = data_msg->temp_val;
				relay_data_store[idx].hum  = data_msg->hum_val;
				relay_data_store[idx].soil = data_msg->soil_val;
				relay_data_store[idx].has_data = 1;
					// printf("[RELAY] Data saved to Slot %d\r\n", idx); // Debug
			} else {
				printf("[RELAY] Error: Sensor ID 0x%02X managed but not found in store!\r\n", data_msg->sensor_id);
			}
        }
    }
}


/*
 * @brief:  Gửi (Broadcast) ACK cho các Sensor đang nằm trong hàng đợi (Timeout: RELAY_ACK_WINDOW_MS)
 * 			Bao gồm cấp phát timeslot cho TDMA và Cycle tổng (total_cycle) cho từng Sensor
 * 			[Func | RelayID | Sensor_ID | TDMA slot | total_cycle]
 * @param:
 * 			_lora: Con trỏ struct LoRa quản lý
 * 			_myRelayID: ID Relay node
 * 			_queue: Hàng chờ yêu cầu Đăng ký của Sensor node
 */

// --- TASK 1: GỬI ACK (Fixed Time: RELAY_ACK_WINDOW_MS) ---
void LoRaApp_Relay_Task_SendACKs(LoRa* _lora, uint8_t _myRelayID, Relay_Reg_Queue_t* _queue) {
    uint32_t start_task = HAL_GetTick();

    // Logic gửi ACK
    if (_queue->count > 0) {
        uint8_t tx_buf[10];
        msg_ss_reg_ack_t ack_msg;
//        printf("[RELAY] Sending %d ACKs...\r\n", _queue->count);

        for (int i = 0; i < _queue->count; i++) {
            uint8_t sensor_id = _queue->pending_sensors[i];
            // Cấp time slot cho sensor node
            int slot_idx = GetSensorIndex(sensor_id);
            if (slot_idx == -1) slot_idx = 0;

            ack_msg.func_code = FUNC_CODE_REG_ACK;
            ack_msg.relay_id = _myRelayID;
            ack_msg.target_sensor_id = sensor_id;
            ack_msg.time_slot = (uint8_t)slot_idx;

            ack_msg.total_cycle = (uint8_t)TOTAL_CYCLE_SEC;

            memcpy(tx_buf, &ack_msg, sizeof(msg_ss_reg_ack_t));

            LoRa_transmit(_lora, tx_buf, sizeof(msg_ss_reg_ack_t), 200);

            int result;

            // Broadcast + nhắc lại 2 lần
            for (int i = 0; i < 2; i++){
            	result = LoRa_transmit(_lora, tx_buf, sizeof(msg_ss_reg_ack_t), 100);
            	HAL_Delay(20);
            }
            if (result){
            	printf("[RELAY] Sending %d ACKs... -> OK\r\n", _queue->count);
            } else {
            	printf("[RELAY] Sending %d ACKs... -> FAILED\r\n", _queue->count);
            }
        }
        _queue->count = 0;
    }

    // Bù giờ cho đủ  Timeout RELAY_ACK_WINDOW_MS
    Pad_Execution_Time(start_task, RELAY_ACK_WINDOW_MS);
    LoRa_setMode(_lora, RXCONTIN_MODE); // Chuyển sang nghe
}


/*
 * @brief:  Gom/tạo bản tin tổng hợp dữ liệu cac Sensor node quản lý và forward tới GW (Timeout: RELAY_GW_WINDOW_MS)
 * 			[Func | RelayID | SensorID_1 | Temp_1 | Humid_1 | Soil_1 | ... | SensorID_n | Temp_n | Humid_n | Soil_n |]
 * @param:
 * 			_lora: Con trỏ struct LoRa quản lý
 * 			_myRelayID: ID Relay node
 * 			_queue: Hàng chờ yêu cầu Đăng ký của Sensor node
 */

// --- TASK 3: FORWARD GATEWAY (Fixed Time: RELAY_GW_WINDOW_MS) ---
void LoRaApp_Relay_Task_ForwardToGateway(LoRa* _lora, uint8_t _myRelayID) {
    uint32_t start_task = HAL_GetTick();
    uint8_t tx_buf[256];
    uint8_t idx = 0;

    // Gom bản tin
    tx_buf[idx++] = FUNC_CODE_RL_DATA;
    tx_buf[idx++] = _myRelayID;

    uint8_t has_data = 0;
    for(int i=0; i<MANAGED_SENSOR_COUNT; i++) {
        if(relay_data_store[i].has_data) {
            tx_buf[idx++] = relay_data_store[i].sensor_id;
            tx_buf[idx++] = (relay_data_store[i].temp >> 8) & 0xFF;
            tx_buf[idx++] = (relay_data_store[i].temp) & 0xFF;
            tx_buf[idx++] = (relay_data_store[i].hum >> 8) & 0xFF;
            tx_buf[idx++] = (relay_data_store[i].hum) & 0xFF;
            tx_buf[idx++] = relay_data_store[i].soil;
            has_data = 1;
        }
    }

    // Gửi & Chờ ACK (Nếu có dữ liệu)
    if (has_data) {
        printf("[RELAY] Forwarding to GW (%d bytes)...\r\n", idx);

//        // Debug bản tin HEX
//        printf("HEX: ");
//        for(int k=0; k<idx; k++) printf("%02X ", tx_buf[k]);
//        printf("\r\n");

        LoRa_setMode(_lora, STNBY_MODE);
        LoRa_transmit(_lora, tx_buf, idx, 500); // Timeout gửi 500ms

        // Chờ ACK (Thời gian còn lại trong window)
        LoRa_setMode(_lora, RXCONTIN_MODE);

        // Tính thời gian còn lại để chờ ACK
        uint32_t elapsed = HAL_GetTick() - start_task;
        uint32_t remaining = (RELAY_GW_WINDOW_MS > elapsed) ? (RELAY_GW_WINDOW_MS - elapsed) : 0;

        uint32_t wait_start = HAL_GetTick();
        uint8_t rx_gw[10];
        extern volatile uint8_t loraRxDoneFlag;

        while(HAL_GetTick() - wait_start < remaining) {
            if(loraRxDoneFlag) {
                loraRxDoneFlag = 0;
                if(LoRa_receive(_lora, rx_gw, sizeof(rx_gw)) > 0) {
                    if(rx_gw[0] == FUNC_CODE_GW_ACK) {
                        printf("[RELAY] GW ACK OK.\r\n");
                        break;
                    }
                }
            }
        }
    } else {
        printf("[RELAY] No Data to Forward.\r\n");
    }

    // Bù giờ cho đủ timeout RELAY_GW_WINDOW_MS
    Pad_Execution_Time(start_task, RELAY_GW_WINDOW_MS);
}

#endif


#if (CURRENT_NODE_TYPE == NODE_TYPE_GATEWAY)
// ==============================
// --- HÀM PHÍA GATEWAY ---
// ==============================

static Gateway_Relay_List_t gw_relay_list;

void LoRaApp_Gateway_Init(void) {
    gw_relay_list.count = 0;
    printf("[GW] Gateway Initialized. Start listening ...\r\n");
}

/*
 * @brief: Xử lý bản tin nhận được tại Gateway
 */
void LoRaApp_Gateway_RxProcessing(LoRa* _lora, uint8_t* _rxBuf, uint8_t len) {
    uint8_t func_code = _rxBuf[0];

    // --- XỬ LÝ RELAY ĐĂNG KÝ (0x06) ---
    if (func_code == FUNC_CODE_RL_REG_ADV) {
        msg_rl_reg_adv_t* adv = (msg_rl_reg_adv_t*)_rxBuf;

        // Kiểm tra xem ID đã có trong danh sách chưa
        uint8_t known = 0;
        for(int i=0; i<gw_relay_list.count; i++) {
            if(gw_relay_list.relays[i].relay_id == adv->relay_id) {
                gw_relay_list.relays[i].last_seen = HAL_GetTick(); // Update timestamp
                known = 1;
                break;
            }
        }

        if(!known && gw_relay_list.count < MAX_RELAY_QUEUE) {
            gw_relay_list.relays[gw_relay_list.count].relay_id = adv->relay_id;
            gw_relay_list.relays[gw_relay_list.count].last_seen = HAL_GetTick();
            gw_relay_list.count++;
//            printf("[GW] New Relay Registered: 0x%02X\r\n", adv->relay_id);
        }
    }
    // --- XỬ LÝ DỮ LIỆU BÁO CÁO TỪ RELAY (0x04) ---
    else if (func_code == FUNC_CODE_RL_DATA) {
    	uint8_t relay_id = _rxBuf[1];
		uint8_t sensor_count = _rxBuf[2];
		uint8_t ptr = 3;

		printf("DATA");

		// Duyệt qua từng sensor trong gói tin này
		for(int i=0; i<sensor_count; i++) {
			// Kiểm tra bounds
			if(ptr + 6 > len) break;

			uint8_t s_id = _rxBuf[ptr];
			int16_t temp = (_rxBuf[ptr+1] << 8) | _rxBuf[ptr+2];
			uint16_t hum = (_rxBuf[ptr+3] << 8) | _rxBuf[ptr+4];
			uint8_t soil = _rxBuf[ptr+5];

			// In ra UART theo định dạng CSV
			// Format:SensorID,Temp,Hum,Soil,..
			printf(",0x%02X,%.1f,%.1f,%d", s_id, temp/10.0, hum/10.0, soil);
			ptr += 6; // Nhảy 6 byte (1 ID + 2 Temp + 2 Hum + 1 Soil)
		}

		//Đánh dấu kết thúc
		printf("\r\n");
    }
}

/*
 * @brief: Gửi danh sách Relay định kỳ qua UART
 */
void LoRaApp_Gateway_Send_RL_Queue(void) {
    if(gw_relay_list.count == 0) return;
//    printf("[GW-DEBUG] Registered Relays: ");
    printf("ADV,");
    for(int i=0; i<gw_relay_list.count; i++) {
        printf("0x%02X", gw_relay_list.relays[i].relay_id);
        if(i < gw_relay_list.count - 1) printf(", ");
    }
    printf("\r\n");
}

/*
 * @brief: Parse lệnh UART và Broadcast cấu hình xuống Relay
 * Input format: "total_cycle,ID1,dt1,ID2,dt2..."
 */

void LoRaApp_Gateway_ProcessConfigCommand(LoRa* _lora, char* cmd_str){
	uint8_t tx_buf[255];
	uint8_t idx = 0;

	//Tách chuỗi lấy total_cycle
	char* token = strtok(cmd_str, ",");
	uint16_t total_cycle = (uint16_t)atoi(token);

	//Đóng gói bản tin ACK (GW->RL)
	// Header: [Func (1) | Cycle_H (1) | Cycle_L (1) | Count (1)]
	tx_buf[idx++] = FUNC_CODE_GW_REG_ACK;
	tx_buf[idx++] = (total_cycle >> 8) & 0xFF;
	tx_buf[idx++] = (total_cycle) & 0xFF;

	uint8_t count_idx = idx++;
	uint8_t pair_count = 0;

	// 3. Lấy các cặp (RelayID, Delta_t)
	while(token != NULL) {
	        token = strtok(NULL, ","); // ID
	        if(token == NULL) break;
	        uint8_t r_id = (uint8_t)atoi(token);

	        token = strtok(NULL, ","); // Delta_t
	        if(token == NULL) break;
	        uint16_t r_delta_t = (uint16_t)atoi(token);

	        tx_buf[idx++] = r_id;
	        tx_buf[idx++] = (r_delta_t >> 8) & 0xFF;
	        tx_buf[idx++] = (r_delta_t) & 0xFF;
	        pair_count++;
	    }

	// Cập nhật số lượng Relay vào byte Count
	tx_buf[count_idx] = pair_count;

	// 4. Broadcast qua LoRa
	printf("[GW] Broadcasting Config (Cycle: %ds, Nodes: %d)...\r\n", total_cycle, pair_count);
	LoRa_setMode(_lora, STNBY_MODE);
	LoRa_transmit(_lora, tx_buf, idx, 2000);
	LoRa_setMode(_lora, RXCONTIN_MODE);
}

#endif




// ============================================================
// --- RANGE TEST FUNCTIONS ---
// ============================================================

/*
 * @brief: Hàm Test phát (Dành cho Sensor cầm đi xa)
 * Gửi bản tin ADV liên tục kèm số thứ tự gói tin để biết có bị mất gói không
 */
void LoRaApp_TestRange_Tx(LoRa* _lora, uint8_t _myID) {
    static uint32_t packet_counter = 0;
    msg_ss_reg_adv_t tx_msg;

    printf("\r\n[RANGE TEST] TX Started...\r\n");

    while(1) {
        // 1. Đóng gói (Mượn cấu trúc ADV)
        tx_msg.func_code = 0xAA; // Mã Test riêng (0xAA) để Relay dễ lọc
        tx_msg.sensor_id = _myID;
        // Dùng trường target_relay_id để gửi 1 byte thấp của bộ đếm (để check mất gói)
        tx_msg.target_relay_id = (uint8_t)(packet_counter & 0xFF);

        // 2. Gửi
        LoRa_setMode(_lora, STNBY_MODE);
        LoRa_transmit(_lora, (uint8_t*)&tx_msg, sizeof(msg_ss_reg_adv_t), 1000);

        printf("[TX] Pkt: %lu | Size: %d bytes\r\n", packet_counter, sizeof(msg_ss_reg_adv_t));

        packet_counter++;

        // 3. Nghỉ 100ms (Gửi nhanh)
        HAL_Delay(100);
    }
}

/*
 * @brief: Hàm Test thu
 * In ra RSSI (Cường độ tín hiệu) và Packet SNR (Tỷ lệ tín hiệu/nhiễu)
 */
void LoRaApp_TestRange_Rx(LoRa* _lora, uint8_t* _rxBuf, volatile uint8_t* _rxFlag) {

    // Đảm bảo đang ở chế độ nhận
    LoRa_setMode(_lora, RXCONTIN_MODE);

    if (*_rxFlag) {
        *_rxFlag = 0; // Xóa cờ ngắt
        memset(_rxBuf, 0, 20); // Xóa buffer

        int len = LoRa_receive(_lora, _rxBuf, 20);

        if (len > 0) {
            // Kiểm tra xem có phải gói Test (0xAA) không
            if (_rxBuf[0] == 0xAA) {
//                uint8_t sender = _rxBuf[1];
                uint8_t pkt_cnt = _rxBuf[2]; // Byte thấp của counter

                // Lấy thông số tín hiệu từ Struct LoRa
                int rssi = LoRa_getRSSI(_lora);
//                int snr  = _lora->packetSnr;

                printf("[RX] From Range packet | Count: %3d | RSSI: %d dBm\r\n",
                        pkt_cnt, rssi);

//                // Nháy LED báo nhận
//                HAL_GPIO_TogglePin(LED_PORT, LED_PIN);
            }
        }
    }
}
#if (CURRENT_NODE_TYPE == NODE_TYPE_GATEWAY)
// ==============================
// --- HÀM PHÍA GATEWAY ---
// ==============================

static Gateway_Relay_List_t gw_relay_list;

void LoRaApp_Gateway_Init(void) {
    gw_relay_list.count = 0;
//    printf("[GW] Gateway Initialized. Start listening ...\r\n");
}

/*
 * @brief: 	Xử lý bản tin nhận được tại Gateway (Đăng ký và Báo cáo từ Relay)
 * @param:
 * 			_lora:	Con trỏ struct LoRa quản lý
 * 			_rxBuf:	Con trỏ buffer nhận
 * 			len: Độ dài buffer nhận
 */
void LoRaApp_Gateway_RxProcessing(LoRa* _lora, uint8_t* _rxBuf, uint8_t len) {
    uint8_t func_code = _rxBuf[0];

    // --- XỬ LÝ RELAY ĐĂNG KÝ (0x06) ---
    if (func_code == FUNC_CODE_RL_REG_ADV) {
        msg_rl_reg_adv_t* adv = (msg_rl_reg_adv_t*)_rxBuf;

        // Kiểm tra xem ID đã có trong danh sách chưa
        uint8_t known = 0;
        for(int i=0; i<gw_relay_list.count; i++) {
            if(gw_relay_list.relays[i].relay_id == adv->relay_id) {
                gw_relay_list.relays[i].last_seen = HAL_GetTick(); // Update timestamp
                known = 1;
                break;
            }
        }

        if(!known && gw_relay_list.count < MAX_RELAY_QUEUE) {
            gw_relay_list.relays[gw_relay_list.count].relay_id = adv->relay_id;
            gw_relay_list.relays[gw_relay_list.count].last_seen = HAL_GetTick();
            gw_relay_list.count++;
            printf("[GW] New Relay Registered: 0x%02X\r\n", adv->relay_id);
        }
    }
    // --- XỬ LÝ DỮ LIỆU BÁO CÁO TỪ RELAY (0x04) ---
    else if (func_code == FUNC_CODE_RL_DATA) {
    	uint8_t relay_id = _rxBuf[1];
		uint8_t sensor_count = _rxBuf[2];
		uint8_t ptr = 3;

		printf("DATA,0x%02X", relay_id);

		// Duyệt qua từng sensor trong gói tin này
		for(int i=0; i<sensor_count; i++) {
			// Kiểm tra bounds
			if(ptr + 6 > len) break;

			uint8_t s_id = _rxBuf[ptr];
			int16_t temp = (_rxBuf[ptr+1] << 8) | _rxBuf[ptr+2];
			uint16_t hum = (_rxBuf[ptr+3] << 8) | _rxBuf[ptr+4];
			uint8_t soil = _rxBuf[ptr+5];

			// In ra UART theo định dạng CSV
			// Format:SensorID,Temp,Hum,Soil,..
			printf(",0x%02X,%.1f,%.1f,%d", s_id, temp/10.0, hum/10.0, soil);
			ptr += 6; // Nhảy 6 byte (1 ID + 2 Temp + 2 Hum + 1 Soil)
		}

		//Đánh dấu kết thúc
		printf("\r\n");
    }
}


/*
 * @brief: 	Gửi danh sách hàng chờ Relay đăng ký định kỳ qua UART
 * 			[ADV,RelayID_1,RelayID_2,...,RelayID_n]
 */
void LoRaApp_Gateway_Send_RL_Queue(void) {
    if(gw_relay_list.count == 0) return;
//    printf("[GW-DEBUG] Registered Relays: ");
    printf("ADV,");
    for(int i=0; i<gw_relay_list.count; i++) {
        printf("0x%02X", gw_relay_list.relays[i].relay_id);
        if(i < gw_relay_list.count - 1) printf(", ");
    }
    printf("\r\n");
}


/*
 * @brief: 	Parse lệnh UART và Broadcast cấu hình xuống Relay
 * 			Input format: "total_cycle,ID1,dt1,ID2,dt2..."
 *
 * @param:
 * 			_lora: Con trỏ struct LoRa quản lý
 * 			cmd_str: Con trỏ lệnh (char) từ vxl trung tâm qua UART
 */

void LoRaApp_Gateway_ProcessConfigCommand(LoRa* _lora, char* cmd_str){
	uint8_t tx_buf[255];
	uint8_t idx = 0;

	// Xóa queue cũ
	gw_relay_list.count = 0;
	memset(gw_relay_list.relays, 0, sizeof(gw_relay_list.relays));

	printf(">> \"%s\"\r\n", cmd_str);

	// Tách chuỗi lấy total_cycle
	char* token = strtok(cmd_str, ",");
	uint16_t total_cycle = (uint16_t)atoi(token);

	// Đóng gói bản tin ACK (GW->RL)
	// Header: [Func (1) | Cycle_H (1) | Cycle_L (1) | Count (1)]
	tx_buf[idx++] = FUNC_CODE_GW_REG_ACK;
	tx_buf[idx++] = (total_cycle >> 8) & 0xFF;
	tx_buf[idx++] = (total_cycle) & 0xFF;

	uint8_t count_idx = idx++;
	uint8_t pair_count = 0;

	// Lấy các cặp (RelayID, Delta_t)
	while(token != NULL) {
	        token = strtok(NULL, ","); // ID
	        if(token == NULL) break;
	        uint8_t r_id = (uint8_t)strtol(token, NULL, 0);

	        token = strtok(NULL, ","); // Delta_t
	        if(token == NULL) break;
	        uint16_t r_delta_t = (uint16_t)strtol(token, NULL, 0);

	        tx_buf[idx++] = r_id;
	        tx_buf[idx++] = (r_delta_t >> 8) & 0xFF;
	        tx_buf[idx++] = (r_delta_t) & 0xFF;
	        pair_count++;
	    }

	// Cập nhật số lượng Relay vào byte Count
	tx_buf[count_idx] = pair_count;

	// Broadcast qua LoRa
	printf("[GW] Broadcasting Config (Cycle: %ds, Nodes: %d)...\r\n", total_cycle, pair_count);

	int result;
	for (int i = 0; i < 5; i++) {
		// Chuyển sang Standby để nạp FIFO
		LoRa_setMode(_lora, STNBY_MODE);
		// Gửi gói tin
		result = LoRa_transmit(_lora, tx_buf, idx, 2000);
		HAL_Delay(100);
	}

	if (result) {
		printf("Broadcast -> OK\r\n");
	} else {
		printf("Broadcast -> FAILED\r\n");
	}

	LoRa_setMode(_lora, RXCONTIN_MODE);
}

#endif


// ============================================================
// --- RANGE TEST FUNCTIONS ---
// ============================================================

/*
 * @brief: 	Hàm Test tầm xa (Bên Phát - Transmitter)
 * 			Gửi bản tin ADV liên tục kèm số thứ tự gói tin
 *
 * @param:
 * 			_lora: Con trỏ struct LoRa quản lý
 * 			_myID: ID node phát
 */
void LoRaApp_TestRange_Tx(LoRa* _lora, uint8_t _myID) {
    static uint32_t packet_counter = 0;
    msg_ss_reg_adv_t tx_msg;

    printf("\r\n[RANGE TEST] TX Started...\r\n");

    while(1) {
        // Đóng gói (theo cấu trúc bản tin ADV)
        tx_msg.func_code = 0xAA; // Mã Test riêng 0xAA
        tx_msg.sensor_id = _myID;

        // Dùng trường target_relay_id để gửi 1 byte bộ đếm
        tx_msg.target_relay_id = (uint8_t)(packet_counter & 0xFF);

        // Broadcast bản tin Range test
        LoRa_setMode(_lora, STNBY_MODE);
        LoRa_transmit(_lora, (uint8_t*)&tx_msg, sizeof(msg_ss_reg_adv_t), 1000);

        printf("[TX] Pkt: %lu | Size: %d bytes\r\n", packet_counter, sizeof(msg_ss_reg_adv_t));

        packet_counter++;

        // Delay 100 ms
        HAL_Delay(100);
    }
}

/*
 * @brief: 	Hàm Test Tầm xa (Bên Thu - Reciever)
 * 			Xử lý khi nhận được bản tin Range Test từ node phát và in ra RSSI (Cường độ tín hiệu)
 * @param:
 * 			_lora: Con trỏ struct LoRa quản lý
 * 			_rxBuf: Con trỏ buffer nhận
 * 			_rxFlag: Cờ nhận (recieve flag)
 */
void LoRaApp_TestRange_Rx(LoRa* _lora, uint8_t* _rxBuf, volatile uint8_t* _rxFlag) {

    // Đảm bảo đang ở chế độ nhận
    LoRa_setMode(_lora, RXCONTIN_MODE);

    if (*_rxFlag) {
        *_rxFlag = 0; // Xóa cờ ngắt
        memset(_rxBuf, 0, 20); // Xóa buffer

        int len = LoRa_receive(_lora, _rxBuf, 20);

        if (len > 0) {
            // Kiểm tra xem có phải gói Test (0xAA) không
            if (_rxBuf[0] == 0xAA) {
//                uint8_t sender = _rxBuf[1];
                uint8_t pkt_cnt = _rxBuf[2]; // Byte thấp của counter

                // Lấy thông số tín hiệu từ Struct LoRa
                int rssi = LoRa_getRSSI(_lora);
//                int snr  = _lora->packetSnr;

                printf("[RX] From Range packet | Count: %3d | RSSI: %d dBm",
                        pkt_cnt, rssi);

                // Nháy LED báo nhận
                HAL_GPIO_TogglePin(LED_PORT, LED_PIN);
            }
        }
    }
}
