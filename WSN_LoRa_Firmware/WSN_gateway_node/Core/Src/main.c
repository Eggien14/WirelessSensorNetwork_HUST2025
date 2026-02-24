/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "rtc.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "sx1278_lora.h"
#include "lora_app.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

LoRa myLoRa;

uint8_t txBuffer[128];
uint8_t rxBuffer[128];

//Cờ xử lý nhận LoRa và ngắt Alarm
volatile uint8_t loraRxDoneFlag = 0;
volatile uint8_t alarm_triggered = 0;

// Bộ đệm xử lý UART (Nhận lệnh từ ESP32)
#define UART_RX_BUF_SIZE 256
uint8_t uartRxBuffer[UART_RX_BUF_SIZE]; //Buffer nhận raw
uint8_t cmdBuffer[UART_RX_BUF_SIZE];    //Buffer chứa lệnh hoàn chỉnh
volatile uint8_t uartRxIndex = 0;		//Index vị trí hiện tại
volatile uint8_t cmdReadyFlag = 0;      //Cờ báo có lệnh mới

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

uint8_t initialize_lora() {

    myLoRa.CS_port = SPI1_CS_GPIO_Port;
    myLoRa.CS_pin = SPI1_CS_Pin;
    myLoRa.reset_port = SPI1_RST_GPIO_Port;
    myLoRa.reset_pin = SPI1_RST_Pin;
    myLoRa.DIO0_port = DIO0_GPIO_Port;
    myLoRa.DIO0_pin = DIO0_Pin;
    myLoRa.hSPI = &hspi1;

    // --- Cấu hình LoRa ---
    myLoRa.frequency = 433;
    myLoRa.spredingFactor = SF_7;
    myLoRa.bandWidth = BW_125KHz;
    myLoRa.crcRate = CR_4_5;
    myLoRa.power = POWER_20db;
    myLoRa.overCurrentProtection = 140;
    myLoRa.preamble = 8;

    // --- Thực hiện Reset và Init ---
    LoRa_reset(&myLoRa);
    uint16_t init_result = LoRa_init(&myLoRa);
    if (init_result != LORA_OK) {
        return 0;
    }
    printf("LoRa Init OK.");
    HAL_GPIO_WritePin(LED_PORT, LED_PIN, 1);
    return 1;
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_SPI1_Init();
  MX_TIM4_Init();
  MX_RTC_Init();
  MX_USART2_UART_Init();
  MX_TIM1_Init();
  /* USER CODE BEGIN 2 */

  __HAL_RCC_PWR_CLK_ENABLE();

  // --- LORA INIT ---
  if(initialize_lora() == 0) {
	  printf("LoRa Init Failed!\r\n");
//	  Error_Handler();
  }

  // Khởi tạo Gateway App
  LoRaApp_Gateway_Init();

  // Bắt đầu nhận UART (Ngắt từng byte)
  HAL_UART_Receive_IT(&huart2, &uartRxBuffer[0], 1);

  // Chuyển LoRa sang chế độ nhận liên tục
  LoRa_setMode(&myLoRa, RXCONTIN_MODE);

  printf("\r\n[GW] >>> INIT OK! START RECIEVING <<<\r\n");

  uint32_t last_queue_time = 0;

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
	  // XỬ LÝ GÓI TIN LORA ĐẾN
	if (loraRxDoneFlag) {
		loraRxDoneFlag = 0;
		memset(rxBuffer, 0, sizeof(rxBuffer));

		int len = LoRa_receive(&myLoRa, rxBuffer, sizeof(rxBuffer));
		if (len > 0) {
			// Hàm xử lý bản tin GW nhận được (Đăng ký Relay hoặc Data Relay)
			LoRaApp_Gateway_RxProcessing(&myLoRa, rxBuffer, len);
		}
	}

	// XỬ LÝ LỆNH CẤU HÌNH TỪ UART (ESP32 GỬI XUỐNG)
	if (cmdReadyFlag) {
		cmdReadyFlag = 0; // Xóa cờ

		// Format lệnh mong đợi: "60,0x01,50,0x02,10" (TotalCycle, ID1, Offset1...)
		LoRaApp_Gateway_ProcessConfigCommand(&myLoRa, (char*)cmdBuffer);
	}

	// GỬI QUEUE ĐỊNH KỲ (Mỗi 5s)
	if (HAL_GetTick() - last_queue_time > 5000) {
		// In danh sách Relay đang quản lý
		LoRaApp_Gateway_Send_RL_Queue();
		last_queue_time = HAL_GetTick();
	}

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE|RCC_OSCILLATORTYPE_LSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_RTC;
  PeriphClkInit.RTCClockSelection = RCC_RTCCLKSOURCE_LSE;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

// Hàm printf qua UART2
#ifdef __GNUC__
#define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
#else
#define PUTCHAR_PROTOTYPE int fputc(int ch, FILE *f)
#endif

PUTCHAR_PROTOTYPE {
    HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
    return ch;
}

//===================== CALLBACK XỬ LÝ NGẮT DIO0 ===========================
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == myLoRa.DIO0_pin) {
        loraRxDoneFlag = 1;
//        printf("[GW] DIO0 Interrupt Triggered!\r\n");
    }

}

void HAL_RTC_AlarmAEventCallback(RTC_HandleTypeDef *hrtc)
{
//    // Bật lại SysTick
//    HAL_ResumeTick();
//
//    // Khôi phục lại Clock
//    SystemClock_Config_FromStop();

	alarm_triggered = 1;
	// Nháy đèn nhanh để biết ngắt đã vào
	HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);

}

// Callback xử lý nhận UART từng byte (để detect \n)
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART2) {
        uint8_t received_char = uartRxBuffer[uartRxIndex];

        // Nếu gặp ký tự xuống dòng -> Kết thúc lệnh
        if (received_char == '\n' || received_char == '\r') {
        	if (uartRxIndex > 0) {
				uartRxBuffer[uartRxIndex] = 0; // Null terminate string

				// Copy sang buffer xử lý để giải phóng buffer nhận
				strcpy((char*)cmdBuffer, (char*)uartRxBuffer);
				cmdReadyFlag = 1;
        	}
        	// Reset index
        	uartRxIndex = 0;
        }
        else {
            uartRxIndex++;
            if (uartRxIndex >= UART_RX_BUF_SIZE) uartRxIndex = 0; // Tràn buffer thì ghi đè từ đầu
        }

        // Tiếp tục nhận byte tiếp theo vào vị trí index mới
        HAL_UART_Receive_IT(&huart2, &uartRxBuffer[uartRxIndex], 1);
    }
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
