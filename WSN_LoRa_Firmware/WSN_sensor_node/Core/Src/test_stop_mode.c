/*
 * test_stop_mode.c
 *
 *  Created on: Dec 25, 2025
 *      Author: ADMIN
 */

/* test_stop_mode.c - Test RTC Stop Mode với UART */

#include "test_stop_mode.h"

extern RTC_HandleTypeDef hrtc;
extern UART_HandleTypeDef huart2;



// Hàm delay thủ công
void manual_delay_ms(uint32_t ms) {
    uint32_t start = HAL_GetTick();
    while (HAL_GetTick() - start < ms);
}

// Hàm test chính
void Test_STOP_Mode(void) {
    char msg[50];

    // Delay 5000ms để đảm bảo an toàn cho lần nạp code sau
    HAL_Delay(5000);

    // Báo init ok
    printf("\r\n=== INIT OK - STM32F103C8T6 ===\r\n");
    printf("Clock: %lu Hz\r\n", HAL_RCC_GetSysClockFreq());

    // Báo ngủ 5s
    printf("Setting RTC Alarm for 5 seconds...\r\n");

    // Cài đặt RTC alarm 5 giây
    RTC_SetAlarm_In_Seconds(5);

//    // Nhấp nháy LED 3 lần trước khi ngủ
//    for(int i = 0; i < 3; i++) {
//        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
//        HAL_Delay(200);
//    }

    printf("Entering STOP mode NOW...\r\n");
    printf("(Chip will sleep, UART will stop)\r\n\r\n");
    HAL_Delay(200);  // Đợi UART truyền xong

    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);

    // Vào STOP mode
    Enter_Stop_Mode();

    // Sau khi thức dậy
//    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
    printf("=== WOKE UP from STOP mode! ===\r\n");
    printf("Clock restored to: %lu Hz\r\n", HAL_RCC_GetSysClockFreq());
    printf("Test completed successfully!\r\n");
}


void Test_RTC_Alive(void) {
    printf("\r\n=== TEST 1: CHECKING RTC COUNTER ===\r\n");

    // Đọc giá trị Counter lần 1
    uint32_t count_1 = (hrtc.Instance->CNTH << 16) | hrtc.Instance->CNTL;
    printf("Current Counter: %lu\r\n", count_1);

    printf("Waiting for 3 seconds (using HAL_Delay)...\r\n");
    HAL_Delay(3000); // Chờ 3 giây bằng SysTick

    // Đọc giá trị Counter lần 2
    uint32_t count_2 = (hrtc.Instance->CNTH << 16) | hrtc.Instance->CNTL;
    printf("Current Counter: %lu\r\n", count_2);

    if (count_2 > count_1) {
        printf(">>> RESULT: PASS! RTC is ticking properly.\r\n");
        printf(">>> (Clock source LSE/LSI is OK)\r\n");
    } else {
        printf(">>> REASON: Crystal LSE is dead or not configured.\r\n");
    }
}

void Test_RTC_Alarm_Only(void) {
    printf("\r\n=== TEST 2: CHECKING RTC ALARM INTERRUPT (NO STOP) ===\r\n");

    // Đặt báo thức sau 3 giây
    printf("Setting Alarm for 3 seconds...\r\n");
    RTC_SetAlarm_In_Seconds(3);

    // Chờ đợi trong vòng lặp (giả lập ngủ nhưng vẫn bật CPU)
    uint32_t start = HAL_GetTick();
    alarm_triggered = 0;

    while (HAL_GetTick() - start < 6000) { // Chờ tối đa 6s
        if (alarm_triggered) {
            printf(">>> RESULT: PASS! Alarm Interrupt Triggered!\r\n");
            return;
        }
        printf(".");
        HAL_Delay(500);
    }

    printf("\r\n>>> RESULT: FAIL! Alarm Timeout. No Interrupt received.\r\n");
    printf(">>> CHECK: NVIC Enabled? EXTI Line 17 Enabled? stm32f1xx_it.c has Handler?\r\n");
}
