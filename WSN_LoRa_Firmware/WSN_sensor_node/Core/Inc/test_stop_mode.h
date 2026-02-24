/*
 * test_stop_mode.h
 *
 *  Created on: Dec 25, 2025
 *      Author: ADMIN
 */

#ifndef INC_TEST_STOP_MODE_H_
#define INC_TEST_STOP_MODE_H_

#include <lora_app.h>
#include "main.h"
#include <stdio.h>
#include <string.h>

extern volatile uint8_t alarm_triggered;

void manual_delay_ms(uint32_t ms);
void Test_STOP_Mode(void);

void Test_RTC_Alive(void);
void Test_RTC_Alarm_Only(void);

#endif /* INC_TEST_STOP_MODE_H_ */
