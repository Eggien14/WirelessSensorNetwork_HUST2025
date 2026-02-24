/*
 * sensor_handle.c
 *
 *  Created on: Dec 2, 2026
 *      Author: ADMIN
 */

#include "sensor_handle.h"

/*
 * Sensor_read.c
 */

#include "sensor_handle.h"
#include <stdio.h>

// ============================================
// --- PRIVATE FUNCTIONS ---
// ============================================

// Hàm delay micro giây sử dụng Timer bất kỳ
static void microDelay(TIM_HandleTypeDef* htim, uint16_t us) {
    __HAL_TIM_SET_COUNTER(htim, 0);
    while (__HAL_TIM_GET_COUNTER(htim) < us);
}

// Chuyển GPIO sang Output
static void Set_Pin_Output(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOx, &GPIO_InitStruct);
}

// Chuyển GPIO sang Input
static void Set_Pin_Input(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOx, &GPIO_InitStruct);
}

// Đọc 1 byte từ DHT22
static uint8_t DHT22_ReadByte(Sensor_Config_t* cfg) {
    uint8_t i, j = 0;
    for (i = 0; i < 8; i++) {
        // Chờ chân xuống thấp
        while (HAL_GPIO_ReadPin(cfg->dht_port, cfg->dht_pin) == 0); // Đợi lên High

        // Đợi 40us
        microDelay(cfg->htim_us, 40);

        // Kiểm tra xem chân còn High không?
        if (HAL_GPIO_ReadPin(cfg->dht_port, cfg->dht_pin) == 0) {
            j &= ~(1 << (7 - i)); // Bit 0
        } else {
            j |= (1 << (7 - i));  // Bit 1
            // Đợi chân xuống thấp trở lại
            while (HAL_GPIO_ReadPin(cfg->dht_port, cfg->dht_pin) == 1);
        }
    }
    return j;
}

// Đọc độ ẩm đất
static uint8_t Read_Soil_Moisture(Sensor_Config_t* cfg) {
    uint32_t sum = 0;
    uint8_t samples = 10; // Tăng mẫu lên để mượt hơn

    for (int i = 0; i < samples; i++) {
        HAL_ADC_Start(cfg->hadc_soil);
        if (HAL_ADC_PollForConversion(cfg->hadc_soil, 10) == HAL_OK) {
            sum += HAL_ADC_GetValue(cfg->hadc_soil);
        }
        HAL_Delay(2); // Delay nhỏ giữa các lần đo
    }

    uint16_t adc_avg = sum / samples;

    // HIỆU CHUẨN
    uint16_t dry_val = 4095;
    uint16_t wet_val = 1500;

    if (adc_avg >= dry_val) return 0;
    if (adc_avg <= wet_val) return 100;

    return (uint8_t)(100 - ((adc_avg - wet_val) * 100) / (dry_val - wet_val));
}


// ============================================
// --- PUBLIC FUNCTION  ---
// ============================================

void Sensor_Init(Sensor_Config_t* _config) {
    // Bắt đầu Timer đếm micro giây
    HAL_TIM_Base_Start(_config->htim_us);

    // Calibrate ADC nếu cần (tùy dòng chip, F103 nên calib)
    HAL_ADCEx_Calibration_Start(_config->hadc_soil);
}

void Sensor_ReadAll(Sensor_Config_t* _config, Sensor_Data_t* _data) {
    uint8_t rh1, rh2, tc1, tc2, sum;

    // --- 1. ĐỌC DHT22 ---
    _data->status = 0; // Reset status

    // Start signal
    Set_Pin_Output(_config->dht_port, _config->dht_pin);
    HAL_GPIO_WritePin(_config->dht_port, _config->dht_pin, 0);
    microDelay(_config->htim_us, 1200); // Kéo thấp > 1ms (1.2ms)
    HAL_GPIO_WritePin(_config->dht_port, _config->dht_pin, 1);
    microDelay(_config->htim_us, 30);   // Kéo cao 30us
    Set_Pin_Input(_config->dht_port, _config->dht_pin);

    // Check response
    microDelay(_config->htim_us, 40);
    if (HAL_GPIO_ReadPin(_config->dht_port, _config->dht_pin) == 0) {
        microDelay(_config->htim_us, 80);
        if (HAL_GPIO_ReadPin(_config->dht_port, _config->dht_pin) == 1) {
            microDelay(_config->htim_us, 40); // Đợi sẵn sàng đọc data

            // Đọc 5 bytes
            rh1 = DHT22_ReadByte(_config);
            rh2 = DHT22_ReadByte(_config);
            tc1 = DHT22_ReadByte(_config);
            tc2 = DHT22_ReadByte(_config);
            sum = DHT22_ReadByte(_config);

            // Checksum
            if (sum == ((rh1 + rh2 + tc1 + tc2) & 0xFF)) {
                uint16_t rawHum = (rh1 << 8) | rh2;
                uint16_t rawTemp = (tc1 << 8) | tc2;

                _data->hum_rh = rawHum / 10.0f;

                if (rawTemp & 0x8000) {
                    _data->temp_c = -((rawTemp & 0x7FFF) / 10.0f);
                } else {
                    _data->temp_c = rawTemp / 10.0f;
                }
            } else {
                _data->status = 1; // Checksum Error
            }
        } else {
            _data->status = 2; // Response Error (High missing)
        }
    } else {
        _data->status = 3; // Response Error (Low missing)
    }

    // --- ĐỌC ĐỘ ẨM ĐẤT ---
    _data->soil_percent = Read_Soil_Moisture(_config);
}


/*
 * @brief: Hàm giả lập dữ liệu cảm biến để test logic LoRa
 */
void Sensor_ReadAll_Test(Sensor_Config_t* _config, Sensor_Data_t* _data) {

    // Giả lập Nhiệt độ: Base 18.0, lệch [-1.0 đến 1.0]
    float temp_offset = ((float)rand() / (float)RAND_MAX) * 2.0f - 1.0f;
    _data->temp_c = 18.0f + temp_offset;

    // Làm tròn 1 số sau dấu phẩy
    _data->temp_c = (int)(_data->temp_c * 10) / 10.0f;


    // Giả lập Độ ẩm: Base 40.0, lệch [-2.0 đến 2.0]
    float hum_offset = ((float)rand() / (float)RAND_MAX) * 4.0f - 2.0f;
    _data->hum_rh = 40.0f + hum_offset;

    // Làm tròn 1 số sau dấu phẩy
    _data->hum_rh = (int)(_data->hum_rh * 10) / 10.0f;


    // Giả lập Độ ẩm đất: Base 30, lệch [0 đến 10]
    uint8_t soil_offset = rand() % 11;
    _data->soil_percent = 30 + soil_offset;


    // Set trạng thái OK
    _data->status = 0;
}
