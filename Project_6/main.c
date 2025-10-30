/* USER CODE BEGIN Header */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "FreeRTOS.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "task.h"
#include "lcd.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define CODE_BUFFER_SIZE 21  // Store last 10 codes

/* IR Remote Button Codes */
#define IR_POWER    33441975
#define IR_MODE     33446055
#define IR_MUTE     33454215
#define IR_PLAYPAUSE 33456255
#define IR_BACK     33439935
#define IR_FORWARD  33472575
#define IR_EQ       33431775
#define IR_VOL_MINUS 33464415
#define IR_VOL_PLUS  33448095
#define IR_0       33480735
#define IR_RPT     33427695
#define IR_USD     33460335
#define IR_1       33444015
#define IR_2       33478695
#define IR_3       33486855
#define IR_4       33435855
#define IR_5       33468495
#define IR_6       33452175
#define IR_7       33423615
#define IR_8       33484815
#define IR_9       33462375

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
TIM_HandleTypeDef htim1;
TIM_HandleTypeDef htim3;

UART_HandleTypeDef huart3;

/* USER CODE BEGIN PV */
volatile uint32_t runtimeStatsTimer = 0;
uint32_t tempCode;
uint8_t bitIndex;
uint8_t cmd;
uint8_t cmdli;
uint32_t code;
uint32_t codeBuffer[CODE_BUFFER_SIZE];
uint8_t codeIndex = 0;
char msg[48];
int len = 0;
char msg[48];
uint8_t GetData;
uint8_t FanStatus = 0;
uint8_t LightStatus = 0;
/* Communication between ISR and main */
volatile uint32_t lastCodeFromISR = 0;
volatile uint8_t newCodeFlag = 0;
uint8_t tx_buffer[27] = "SILICON SQUAD ";
uint8_t rx_indx = 0;
uint8_t transfer_cplt = 0;
uint8_t rx_buffer[50];
uint8_t rx_data[1];
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART3_UART_Init(void);
static void MX_TIM1_Init(void);
static void MX_TIM3_Init(void);
/* USER CODE BEGIN PFP */
void irRecv(void *pvParameters);
void motor_config(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void) {
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
	MX_USART3_UART_Init();
	MX_TIM1_Init();
	MX_TIM3_Init();
	/* USER CODE BEGIN 2 */
	HAL_TIM_Base_Start(&htim1);
	__HAL_TIM_SET_COUNTER(&htim1, 0);
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6, GPIO_PIN_SET);

	HAL_UART_Receive_IT(&huart3, rx_data, 1);
	xTaskCreate(irRecv, "IR RECV", configMINIMAL_STACK_SIZE * 2, NULL, 2, NULL);
	vTaskStartScheduler();
	/* USER CODE END 2 */

	/* Infinite loop */
	/* USER CODE BEGIN WHILE */
	while (1) {
		/* USER CODE END WHILE */

		/* USER CODE BEGIN 3 */
	}
	/* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void) {
	RCC_OscInitTypeDef RCC_OscInitStruct = { 0 };
	RCC_ClkInitTypeDef RCC_ClkInitStruct = { 0 };

	/** Configure the main internal regulator output voltage
	 */
	__HAL_RCC_PWR_CLK_ENABLE();
	__HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

	/** Initializes the RCC Oscillators according to the specified parameters
	 * in the RCC_OscInitTypeDef structure.
	 */
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
	RCC_OscInitStruct.HSIState = RCC_HSI_ON;
	RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
	RCC_OscInitStruct.PLL.PLLM = 8;
	RCC_OscInitStruct.PLL.PLLN = 72;
	RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
	RCC_OscInitStruct.PLL.PLLQ = 4;
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
		Error_Handler();
	}

	/** Initializes the CPU, AHB and APB buses clocks
	 */
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
			| RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) {
		Error_Handler();
	}
}

/**
 * @brief TIM1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM1_Init(void) {

	/* USER CODE BEGIN TIM1_Init 0 */

	/* USER CODE END TIM1_Init 0 */

	TIM_ClockConfigTypeDef sClockSourceConfig = { 0 };
	TIM_MasterConfigTypeDef sMasterConfig = { 0 };

	/* USER CODE BEGIN TIM1_Init 1 */

	/* USER CODE END TIM1_Init 1 */
	htim1.Instance = TIM1;
	htim1.Init.Prescaler = 71;
	htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
	htim1.Init.Period = 65535;
	htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	htim1.Init.RepetitionCounter = 0;
	htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
	if (HAL_TIM_Base_Init(&htim1) != HAL_OK) {
		Error_Handler();
	}
	sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
	if (HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig) != HAL_OK) {
		Error_Handler();
	}
	sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
	sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
	if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig)
			!= HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN TIM1_Init 2 */

	/* USER CODE END TIM1_Init 2 */

}

/**
 * @brief TIM3 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM3_Init(void) {

	/* USER CODE BEGIN TIM3_Init 0 */

	/* USER CODE END TIM3_Init 0 */

	TIM_ClockConfigTypeDef sClockSourceConfig = { 0 };
	TIM_MasterConfigTypeDef sMasterConfig = { 0 };
	TIM_OC_InitTypeDef sConfigOC = { 0 };

	/* USER CODE BEGIN TIM3_Init 1 */

	/* USER CODE END TIM3_Init 1 */
	htim3.Instance = TIM3;
	htim3.Init.Prescaler = 71;
	htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
	htim3.Init.Period = 100;
	htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
	if (HAL_TIM_Base_Init(&htim3) != HAL_OK) {
		Error_Handler();
	}
	sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
	if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK) {
		Error_Handler();
	}
	if (HAL_TIM_PWM_Init(&htim3) != HAL_OK) {
		Error_Handler();
	}
	sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
	sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
	if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig)
			!= HAL_OK) {
		Error_Handler();
	}
	sConfigOC.OCMode = TIM_OCMODE_PWM1;
	sConfigOC.Pulse = 0;
	sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
	sConfigOC.OCFastMode = TIM_OCFAST_ENABLE;
	if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_1)
			!= HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN TIM3_Init 2 */

	/* USER CODE END TIM3_Init 2 */
	HAL_TIM_MspPostInit(&htim3);

}

/**
 * @brief USART3 Initialization Function
 * @param None
 * @retval None
 */
static void MX_USART3_UART_Init(void) {

	/* USER CODE BEGIN USART3_Init 0 */
	/* USER CODE END USART3_Init 0 */

	/* USER CODE BEGIN USART3_Init 1 */
	/* USER CODE END USART3_Init 1 */
	huart3.Instance = USART3;
	huart3.Init.BaudRate = 115200;
	huart3.Init.WordLength = UART_WORDLENGTH_8B;
	huart3.Init.StopBits = UART_STOPBITS_1;
	huart3.Init.Parity = UART_PARITY_NONE;
	huart3.Init.Mode = UART_MODE_TX_RX;
	huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	huart3.Init.OverSampling = UART_OVERSAMPLING_16;
	if (HAL_UART_Init(&huart3) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN USART3_Init 2 */
	/* USER CODE END USART3_Init 2 */

}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void) {
	GPIO_InitTypeDef GPIO_InitStruct = { 0 };

	/* GPIO Ports Clock Enable */
	__HAL_RCC_GPIOH_CLK_ENABLE();
	__HAL_RCC_GPIOC_CLK_ENABLE();
	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOB_CLK_ENABLE();

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_6,
			GPIO_PIN_RESET);

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3 | GPIO_PIN_4, GPIO_PIN_RESET);

	/*Configure GPIO pins : PC0 PC1 PC6 */
	GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_6;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

	/*Configure GPIO pins : PA3 PA4 */
	GPIO_InitStruct.Pin = GPIO_PIN_3 | GPIO_PIN_4;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	/*Configure GPIO pin : PB11 */
	GPIO_InitStruct.Pin = GPIO_PIN_11;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

	/*Configure GPIO pins : PB3 PB4 */
	GPIO_InitStruct.Pin = GPIO_PIN_3 | GPIO_PIN_4;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

	/* EXTI interrupt init*/
	HAL_NVIC_SetPriority(EXTI3_IRQn, 2, 0);
	HAL_NVIC_EnableIRQ(EXTI3_IRQn);

	HAL_NVIC_SetPriority(EXTI4_IRQn, 2, 0);
	HAL_NVIC_EnableIRQ(EXTI4_IRQn);

	HAL_NVIC_SetPriority(EXTI15_10_IRQn, 1, 0);
	HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

}

/* USER CODE BEGIN 4 */
void ConfigureRunTimeStatsTimer(void) {
	// Start TIM2 in interrupt or base mode to increment runtimeStatsTimer
	HAL_TIM_Base_Start_IT(&htim1);
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
	/* ===== IR REMOTE INPUT ===== */
	if (GPIO_Pin == GPIO_PIN_11) {
		uint32_t t = __HAL_TIM_GET_COUNTER(&htim1);

		if (t > 8000) {
			tempCode = 0;
			bitIndex = 0;
		} else if (t > 1700) {
			if (bitIndex < 32)
				tempCode |= (1UL << (31 - bitIndex));
			bitIndex++;
		} else if (t > 1000) {
			if (bitIndex < 32)
				tempCode &= ~(1UL << (31 - bitIndex));
			bitIndex++;
		}

		if (bitIndex == 32) {
			cmdli = (uint8_t) (~tempCode);
			cmd = (uint8_t) (tempCode >> 8);

			if (cmdli == cmd) {
				code = tempCode;
				codeBuffer[codeIndex] = code;
				codeIndex = (codeIndex + 1) % CODE_BUFFER_SIZE;

				lastCodeFromISR = code;
				newCodeFlag = 1;
			}
			bitIndex = 0;
		}

		__HAL_TIM_SET_COUNTER(&htim1, 0);
	}

	/* ===== MANUAL OVERRIDE: FAN BUTTON ===== */
	else if (GPIO_Pin == GPIO_PIN_3) {
		FanStatus = !FanStatus;
		HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_SET); // STBY always ON

		if (FanStatus) {
			HAL_GPIO_WritePin(GPIOC, GPIO_PIN_0, GPIO_PIN_SET);
			HAL_GPIO_WritePin(GPIOC, GPIO_PIN_1, GPIO_PIN_RESET);
			HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
			__HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 50);
			//LcdClear();
			//lprint(0x80, "LED");
			lprint(0xC0, "FAN ON 50%");
		} else {
			__HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 0);
			//LcdClear();
			//lprint(0x80, "LED");
			lprint(0xC0, "FAN OFF   ");
		}
	}

	/* ===== MANUAL OVERRIDE: LIGHT BUTTON ===== */
	else if (GPIO_Pin == GPIO_PIN_4) {
		LightStatus = !LightStatus;
		HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4,
				LightStatus ? GPIO_PIN_SET : GPIO_PIN_RESET);

		LcdClear();
		lprint(0x80, LightStatus ? "LED ON " : "LED OFF");
		lprint(0xC0, FanStatus ? "FAN ON    " : "FAN OFF   ");
	}
}

void irRecv(void *pvParameters) {
	LcdInit();
	motor_config();

	lprint(0x80, "LED OFF");
	lprint(0xC0, "FAN OFF   ");

	while (1) {
		//HAL_UART_Transmit(&huart3, tx_buffer, 27, 10);
		if (newCodeFlag) {
			__disable_irq();
			uint32_t codeToProcess = lastCodeFromISR;
			newCodeFlag = 0;
			__enable_irq();

			switch (codeToProcess) {
			case IR_POWER:
				LightStatus = !LightStatus;
				HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4,
						LightStatus ? GPIO_PIN_SET : GPIO_PIN_RESET);
				LcdClear();
				lprint(0x80, LightStatus ? "LED ON " : "LED OFF");
				lprint(0xC0, FanStatus ? "FAN ON    " : "FAN OFF   ");
				break;

			case IR_PLAYPAUSE:
				FanStatus = !FanStatus;
				HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_SET); // STBY ON
				HAL_GPIO_WritePin(GPIOC, GPIO_PIN_0, GPIO_PIN_SET);
				HAL_GPIO_WritePin(GPIOC, GPIO_PIN_1, GPIO_PIN_RESET);

				if (FanStatus) {
					HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
					__HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 50);

					lprint(0xC0, "FAN ON 50%");
				} else {
					__HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 0);

					lprint(0xC0, "FAN OFF   ");
				}
				break;

			case IR_1:
				if (FanStatus) {
					__HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 30);

					lprint(0xC0, "FAN 30%   ");
				}
				break;

			case IR_2:
				if (FanStatus) {
					__HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 50);

					lprint(0xC0, "FAN 50%");
				}
				break;

			case IR_3:
				if (FanStatus) {
					__HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 80);

					lprint(0xC0, "FAN 80%  ");
				}
				break;

			default:
				break;
			}
		}
		vTaskDelay(pdMS_TO_TICKS(100));
	}
}
void motor_config(void) {
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_SET); // STBY enable
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_0, GPIO_PIN_SET); // AIN1
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_1, GPIO_PIN_RESET); // AIN2
	HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
	__HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 0);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
	if (huart->Instance == USART3) {
		// Clear buffer if starting new command
		if (rx_indx == 0) {
			memset(rx_buffer, 0, sizeof(rx_buffer));
		}

		// Store received byte until Enter (Carriage Return)
		if (rx_data[0] != '\r') {
			rx_buffer[rx_indx++] = rx_data[0];
		} else {
			rx_indx = 0;
			transfer_cplt = 1;

			// Echo newline
			HAL_UART_Transmit(&huart3, (uint8_t*) "\n\r", 2, 100);

			// Compare commands and execute
			if (strcmp((char*) rx_buffer, "Light ON") == 0) {
				LightStatus = 1;
				HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);
			} else if (strcmp((char*) rx_buffer, "Light OFF") == 0) {
				LightStatus = 0;
				HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);
			} else if (strcmp((char*) rx_buffer, "Fan OFF") == 0) {
				FanStatus = 0;
				__HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 0);
			} else if (strcmp((char*) rx_buffer, "Fan ON") == 0) {
				FanStatus = 1;
				HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_SET); // STBY ON
				HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
				__HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 25); // 10% speed default
			} else if (strcmp((char*) rx_buffer, "Fan Speed 1") == 0) {
				if (FanStatus)
					__HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 30);
			} else if (strcmp((char*) rx_buffer, "Fan Speed 2") == 0) {
				if (FanStatus)
					__HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 50);
			} else if (strcmp((char*) rx_buffer, "Fan Speed 3") == 0) {
				if (FanStatus)
					__HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 80);
			}

			// Update LCD
			LcdClear();
			lprint(0x80, LightStatus ? "LED ON " : "LED OFF");
			lprint(0xC0, FanStatus ? "FAN ON" : "FAN OFF");
		}

		// Restart UART interrupt for next byte
		HAL_UART_Receive_IT(&huart3, rx_data, 1);

		// Echo received character back
		HAL_UART_Transmit(&huart3, rx_data, 1, 100);
	}
}

/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void) {
	/* USER CODE BEGIN Error_Handler_Debug */
	/* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
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
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
