/*
 * sensor_handle.h
 *
 *  Created on: Jan 2, 2026
 *      Author: ADMIN
 */

#ifndef INC_SENSOR_HANDLE_H_
#define INC_SENSOR_HANDLE_H_

#include "main.h"

#include "tim.h"
#include "adc.h"
#include "stm32f1xx_hal.h"

#include <stdint.h>

// --- STRUCT CẤU HÌNH PHẦN CỨNG CẢM BIẾN ---
typedef struct {
	// Cho DHT22 (Cần Timer để delay us và GPIO)
	TIM_HandleTypeDef* 		htim_us;      // Timer dùng để đếm micro giây
	GPIO_TypeDef* 			dht_port;
	uint16_t            	dht_pin;

	// Cho Độ ẩm đất (Cần ADC)
	ADC_HandleTypeDef* hadc_soil;    // Bộ ADC đọc cảm biến đất
} Sensor_Config_t;


// --- STRUCT DỮ LIỆU ĐẦU RA ---
typedef struct {
    float       temp_c;      // Nhiệt độ (°C)
    float       hum_rh;      // Độ ẩm không khí (%RH)
    uint8_t     soil_percent;// Độ ẩm đất (0-100%)
    uint8_t     status;      // 0: OK, 1: Error (Timeout/Checksum)
} Sensor_Data_t;


// --- FUNCTION PROTOTYPES ---

// Hàm khởi tạo (Bật Timer, Calibrate ADC nếu cần)
void Sensor_Init(Sensor_Config_t* _config);

// Hàm đọc toàn bộ cảm biến (Wrapper function)
void Sensor_ReadAll(Sensor_Config_t* _config, Sensor_Data_t* _data);

void Sensor_ReadAll_Test(Sensor_Config_t* _config, Sensor_Data_t* _data);

#endif /* INC_SENSOR_HANDLE_H_ */
