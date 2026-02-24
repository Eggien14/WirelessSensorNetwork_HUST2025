/*
 * lora_shared_def.h
 *
 *  Created on: Dec 14, 2025
 *      Author: HaiVuQuang
 *      Github: https://github.com/HaiVuQuang
 */

#ifndef INC_LORA_APP_H_
#define INC_LORA_APP_H_

#include "main.h"
#include <stdio.h>
#include "sx1278_lora.h"

#include "rtc.h"

// --- GLOBAL VARIABLES ---
extern RTC_HandleTypeDef hrtc;
extern UART_HandleTypeDef huart2;


// --- ID CONFIGURATION  ---
#define NODE_TYPE_SENSOR    1
#define NODE_TYPE_RELAY     2
#define NODE_TYPE_GATEWAY   3

// >>>>> CHỌN LOẠI NODE TẠI ĐÂY <<<<<
//#define CURRENT_NODE_TYPE   NODE_TYPE_SENSOR
//#define CURRENT_NODE_TYPE   NODE_TYPE_RELAY
#define CURRENT_NODE_TYPE   NODE_TYPE_GATEWAY


// --- CẤU HÌNH ID TỰ ĐỘNG DỰA TRÊN LOẠI NODE ---
#if (CURRENT_NODE_TYPE == NODE_TYPE_SENSOR)
    // --- CẤU HÌNH CHO SENSOR ---
    #define MY_SENSOR_ID        0xFE
    #define TARGET_RELAY_ID     0x01

#elif (CURRENT_NODE_TYPE == NODE_TYPE_RELAY)
    // --- CẤU HÌNH CHO RELAY ---
    #define MY_RELAY_ID         0x01
    // Danh sách sensor chịu quản lý
    #define MANAGED_SENSOR_LIST     {0xFE, 0xFD, 0xFC}
    // Số lượng sensor chịu quản lý
    #define MANAGED_SENSOR_COUNT    3
#elif (CURRENT_NODE_TYPE == NODE_TYPE_GATEWAY)
    #define MY_GATEWAY_ID       0x00
#endif

// --- FUNCTION CODE ---
#define FUNC_CODE_REG_ADV   		0x01 	// Registation phase:	Bản tin ADV từ Sensor -> Relay
#define FUNC_CODE_REG_ACK   		0x02 	// Registation phase:	Xác nhận đăng ký ACK từ Relay -> Sensor

#define FUNC_CODE_SS_DATA      		0x03 	// Report phase:		Gửi data từ Sensor -> Relay
#define FUNC_CODE_RL_DATA			0x04	// Report phase:		Gửi data từ Relay -> Gateway
#define FUNC_CODE_GW_ACK			0x05	// Report phase:		Xác nhận dữ liệu từ Gateway -> Relay

#define FUNC_CODE_RL_REG_ADV    	0x06    // Registation phase:	Bản tin ADV từ Relay -> Gateway
#define FUNC_CODE_GW_REG_ACK    	0x07    // Registation phase:	Xác nhận đăng ký ACK từ Gateway -> Relay


// --- TIMING ---
#define DEFAULT_TOTAL_CYCLE     	25
#define DEFAULT_WAKE_OFFSET     	0

//#define TOTAL_CYCLE_SEC         15      		// Tổng chu kỳ: 15s

//Cấu hình thời gian cho SENSOR
#define REG_TIMEOUT_MS				2000    	// Thời gian chờ ACK của Sensor (Pha Đăng ký)

#define SENSOR_MEASURE_CYCLE    	3       	// Đo mỗi 7 chu kỳ
#define SENSOR_TX_WINDOW_MS     	3000    	// Thời gian dành cho việc Gửi Data (bao gồm cả TDMA delay)
#define SENSOR_MEASURE_WINDOW_MS 	3000   		// Thời gian dành cho việc Đo đạc

#define SENSOR_TDMA_BASE_MS     	1500    	// Thời gian chờ cơ sở (để Relay kịp dậy gửi ACK)
#define SENSOR_TDMA_SLOT_MS     	100     	// Thời gian mỗi slot

//Cấu hình thời gian cho RELAY
#define RELAY_ACK_WINDOW_MS     	1000    	// Task 1: Gửi ACK đăng ký
#define RELAY_RX_WINDOW_MS      	8000    	// Task 2: Lắng nghe Sensor
#define RELAY_GW_WINDOW_MS      	1000    	// Task 3: Gửi Gateway & Chờ ACK
#define MAX_PENDING_ACK     		10

//Cấu hình Relay Queue cho GW
#define MAX_RELAY_QUEUE 20


// --- FRAME STRUCTURE ---
//Bản tin ADV pha Đăng ký (Sensor -> Relay)
typedef struct {
	uint8_t func_code;
	uint8_t sensor_id;
	uint8_t target_relay_id;
}msg_ss_reg_adv_t;

//Bản tin ACK pha Đăng ký ( Relay -> Sensor )
typedef struct {
	uint8_t func_code;
	uint8_t relay_id;
	uint8_t target_sensor_id;
	uint8_t time_slot;
	uint16_t total_cycle;
	uint8_t reserved;
//	uint16_t wake_interval;
}msg_ss_reg_ack_t;

//Bản tin ADV pha Đăng ký (Relay -> Gateway)
typedef struct {
    uint8_t func_code;      // 0x06
    uint8_t relay_id;
    uint8_t reserved;
} __attribute__((packed)) msg_rl_reg_adv_t;

//Bản tin Dữ liệu pha Báo cáo (Sensor -> Relay) - 8 Bytes
typedef struct {
    uint8_t func_code;          // 0x03
    uint8_t sensor_id;
    uint8_t target_relay_id;
    int16_t temp_val;           // Nhiệt độ * 10
    uint16_t hum_val;           // Độ ẩm * 10
    uint8_t soil_val;           // Độ ẩm đất %
} __attribute__((packed)) msg_ss_data_t;


// --- RELAY MANAGEMENT STRUCT ---

//[RELAY]: Quản lý danh sách các Sensor vừa gửi yêu cầu
typedef struct {
    uint8_t pending_sensors[MAX_PENDING_ACK]; // Danh sách ID sensor chờ ACK
    uint8_t count;                            // Số lượng hiện tại
} Relay_Reg_Queue_t;

//[RELAY]: Struct lưu trữ dữ liệu tại Relay để tổng hợp
typedef struct {
    uint8_t sensor_id;
    int16_t temp;
    uint16_t hum;
    uint8_t soil;
    uint8_t has_data; // Cờ báo đã nhận dữ liệu trong chu kỳ này chưa
} Relay_Sensor_Data_Slot_t;

// --- GATEWAY MANAGEMENT STRUCT ---
typedef struct {
    uint8_t relay_id;
    uint32_t last_seen;
} Relay_Info_t;

typedef struct {
    Relay_Info_t relays[MAX_RELAY_QUEUE];
    uint8_t count;
} Gateway_Relay_List_t;

// --- HANDLE FUNCTION ---
void RTC_SetAlarm_In_Seconds(uint32_t seconds);

void Enter_Stop_Mode(void);

void SystemClock_Config_FromStop(void);

void Pad_Execution_Time(uint32_t start_tick, uint32_t target_duration_ms);

// Test range TX
void LoRaApp_TestRange_Tx(LoRa* _lora, uint8_t _myID);

// // Test range RX
void LoRaApp_TestRange_Rx(LoRa* _lora, uint8_t* _rxBuf, volatile uint8_t* _rxFlag);

#if (CURRENT_NODE_TYPE == NODE_TYPE_SENSOR)

#include "sensor_handle.h"

//[SENSOR]: Thực hiện pha Đăng ký (trả về time slot TDMA)
uint8_t LoRaApp_Sensor_RegistrationPhase(
    LoRa* _lora,                 // Con trỏ tới struct LoRa
    uint8_t* _rxBuf,             // Con trỏ tới buffer nhận
    uint16_t _rxBufSize,         // Kích thước buffer
    volatile uint8_t* _rxFlag,   // Con trỏ tới cờ ngắt (quan trọng!)
    uint8_t _myID,               // ID của Sensor
    uint8_t _targetRelayID       // ID của Relay đích
);

//[SENSOR]: Thực hiện gửi data (theo timeslot) pha Báo cáo (Timeout: SENSOR_TX_WINDOW_MS)
void LoRaApp_Sensor_Task_SendData(LoRa* _lora, uint8_t _myID, uint8_t _targetRelayID, uint8_t _mySlot);

//[SENSOR]: Thực hiện đo cảm biến (Timeout: SENSOR_MEASURE_WINDOW_MS)
void LoRaApp_Sensor_Task_Measure(Sensor_Config_t* _sensorCfg);

#endif

#if (CURRENT_NODE_TYPE == NODE_TYPE_RELAY)
//[RELAY]: Xử lý bản tin nhận được ở pha Báo cáo, phiên lắng nghe
// Gọi trong vòng lặp chính khi có cờ ngắt nhận


uint8_t LoRaApp_Relay_RegistrationWithGateway(
		LoRa* _lora, uint8_t* _rxBuf, uint16_t _rxBufSize,
		volatile uint8_t* _rxFlag, uint8_t _myRelayID);

void LoRaApp_Relay_RxProcessing(
    LoRa* _lora,
    uint8_t* _rxBuf,
    uint8_t _myRelayID,
    Relay_Reg_Queue_t* _queue // Con trỏ tới hàng đợi ACK
);

// [RELAY]: Gửi ACK pha Đăng ký cho sensor node (Timeout: RELAY_ACK_WINDOW_MS)
void LoRaApp_Relay_Task_SendACKs(LoRa* _lora, uint8_t _myRelayID, Relay_Reg_Queue_t* _queue);

//[RELAY]: Ghép bản tin từ dữ liệu Relay_Sensor_Data_Slot_t, gửi tới GW (Timeout: RELAY_GW_WINDOW_MS)
void LoRaApp_Relay_Task_ForwardToGateway(LoRa* _lora, uint8_t _myRelayID);

//[RELAY]: Kiểm tra id sensor có thuộc danh sách kiểm soát hay không?
uint8_t IsSensorManaged(uint8_t sensor_id);

//[RELAY]: Cấp phát index cho Sensor node dựa theo số thứ tự trong danh sách
int GetSensorIndex(uint8_t sensor_id);

//[RELAY]: Reset struct quản lý dữ liệu sensor đầu chu kỳ
void LoRaApp_Relay_Init(void);
#endif

#if (CURRENT_NODE_TYPE == NODE_TYPE_GATEWAY)

//[GATEWAY]: Reset/Init danh sách Relay đang quản lý
void LoRaApp_Gateway_Init(void);

//[GATEWAY]: Xử lý bản tin nhận được tại Gateway
void LoRaApp_Gateway_RxProcessing(LoRa* _lora, uint8_t* _rxBuf, uint8_t len);

//[GATEWAY]: Tạo và gửi danh sách hàng chờ Relay đăng ký (định kỳ)
void LoRaApp_Gateway_Send_RL_Queue(void);

//[GATEWAY]: Parse lệnh UART và Broadcast cấu hình xuống Relay
void LoRaApp_Gateway_ProcessConfigCommand(LoRa* _lora, char* cmd_str);

#endif

#endif /* INC_LORA_APP_H_ */
