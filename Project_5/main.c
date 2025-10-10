/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2025 STMicroelectronics.
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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "stdint.h"
#include "stdio.h"
#include "FreeRTOS.h"
#include "task.h"
#include "lcd.h"
#include "string.h"
#include "math.h"
#include "queue.h"
#include "dht11.h"
#include "ds18b20.h"
#include "semphr.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* MPU6050 Defines */
#define MPU6050_ADDR   (0x68 << 1)
#define WHO_AM_I_REG   0x75
#define PWR_MGMT_1     0x6B
#define ACCEL_XOUT_H   0x3B
#define GYRO_XOUT_H    0x43

#define ENGINE_TEMP_THRESHOLD 30
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;

I2C_HandleTypeDef hi2c2;

TIM_HandleTypeDef htim1;
TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim14;

/* USER CODE BEGIN PV */
typedef struct {
	uint8_t address;  // 0x80 = line1, 0xC0 = line2
	char msg[17];     // 16 chars + null
} LCD_Message_t;
QueueHandle_t lcdQueue;

/*PIR Variables*/
uint8_t PIR_Status = 0;  // 0 = No motion, 1 = Motion detected
volatile uint32_t runtimeStatsTimer = 0;
char lastMsg[17] = "";   // store last LCD text (16 chars + null)
char lastLine[2][17] = { { 0 }, { 0 } };

/* MPU6050 variables */
int16_t accel_x, accel_y, accel_z;
float ax, ay, az;
char prevMsg[17] = "";   // store last LCD text (16 chars + null)

/*DHT11 Motor Variables*/
uint32_t analogIn = 0;
uint8_t flag = 0;
float moisture_percent = 0;

DHT11_Data dht;
uint8_t tempflag = 0;

volatile uint8_t pirAlert = 0;
volatile uint8_t accelAlert = 0;  // 1 = warning, 2 = caution
volatile uint8_t tempHumAlert = 0;  // 1 = motor ON condition
volatile uint8_t waterAlert = 0; // 0 = normal, 1 = warning, 2 = caution, 3 = critical
volatile uint8_t engineTempAlert = 0;

uint32_t adcValue;
/*DS18B20*/
int16_t raw = 0;
int16_t temp_int = 0;
int16_t temp_frac = 0;
char buff[20] = { 0 };
SemaphoreHandle_t lcdMutex;
float engine_temperature = 0.0;
uint32_t adcValue = 0;
/* USER CODE BEGIN PV */
void humanDetect(void *pvParameters);
void accidentDetect(void *pvParameters);
void tempHum(void *pvParameters);
void MPU6050_Init(void);
void MPU6050_Read_Accel(void);
void ConfigureRunTimeStatsTimer(void);
void lcdTask(void *pvParameters);
void Temp_motor_config();
void Temperature_monitoring();
void waterDetect(void *pvParameters);
uint8_t DHT11_Read(DHT11_Data *data);
void Water_level_detection();
void MotorB_Init(void);
void engineTemp(void *pvParameters);
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM2_Init(void);
static void MX_I2C2_Init(void);
static void MX_TIM1_Init(void);
static void MX_TIM14_Init(void);
static void MX_ADC1_Init(void);
static void MX_TIM3_Init(void);
/* USER CODE BEGIN PFP */

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
	MX_TIM2_Init();
	MX_I2C2_Init();
	MX_TIM1_Init();
	MX_TIM14_Init();
	MX_ADC1_Init();
	MX_TIM3_Init();
	/* USER CODE BEGIN 2 */
	LcdInit();

	/* Configure and start the timer used for runtime stats (FreeRTOS) */
	ConfigureRunTimeStatsTimer();

	/* NVIC priority for TIM2 (adjust as needed for your system/FreeRTOS config) */
	HAL_NVIC_SetPriority(TIM2_IRQn, 5, 0);
	HAL_NVIC_EnableIRQ(TIM2_IRQn);

	/* Create tasks */
	lcdMutex = xSemaphoreCreateMutex();
	lcdQueue = xQueueCreate(5, sizeof(LCD_Message_t));

	xTaskCreate(lcdTask, "LCD Task", 256, NULL, 4, NULL);

	// Sensor & Alert Tasks
	xTaskCreate(humanDetect, "Human Task", 192, NULL, 3, NULL);
	xTaskCreate(accidentDetect, "Accident Task", 256, NULL, 3, NULL);
	xTaskCreate(tempHum, "TempHum Task", 192, NULL, 2, NULL);
	xTaskCreate(waterDetect, "Water Task", 192, NULL, 2, NULL);
	xTaskCreate(engineTemp, "EngineTemp Task", 256, NULL, 3, NULL);

	// Start scheduler
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
	RCC_OscInitStruct.PLL.PLLN = 84;
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
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) {
		Error_Handler();
	}
}

/**
 * @brief ADC1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_ADC1_Init(void) {

	/* USER CODE BEGIN ADC1_Init 0 */

	/* USER CODE END ADC1_Init 0 */

	ADC_ChannelConfTypeDef sConfig = { 0 };

	/* USER CODE BEGIN ADC1_Init 1 */

	/* USER CODE END ADC1_Init 1 */

	/** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
	 */
	hadc1.Instance = ADC1;
	hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV2;
	hadc1.Init.Resolution = ADC_RESOLUTION_12B;
	hadc1.Init.ScanConvMode = DISABLE;
	hadc1.Init.ContinuousConvMode = DISABLE;
	hadc1.Init.DiscontinuousConvMode = DISABLE;
	hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
	hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
	hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
	hadc1.Init.NbrOfConversion = 1;
	hadc1.Init.DMAContinuousRequests = DISABLE;
	hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
	if (HAL_ADC_Init(&hadc1) != HAL_OK) {
		Error_Handler();
	}

	/** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
	 */
	sConfig.Channel = ADC_CHANNEL_3;
	sConfig.Rank = 1;
	sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES;
	if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN ADC1_Init 2 */

	/* USER CODE END ADC1_Init 2 */

}

/**
 * @brief I2C2 Initialization Function
 * @param None
 * @retval None
 */
static void MX_I2C2_Init(void) {

	/* USER CODE BEGIN I2C2_Init 0 */

	/* USER CODE END I2C2_Init 0 */

	/* USER CODE BEGIN I2C2_Init 1 */

	/* USER CODE END I2C2_Init 1 */
	hi2c2.Instance = I2C2;
	hi2c2.Init.ClockSpeed = 100000;
	hi2c2.Init.DutyCycle = I2C_DUTYCYCLE_2;
	hi2c2.Init.OwnAddress1 = 0;
	hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
	hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
	hi2c2.Init.OwnAddress2 = 0;
	hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
	hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
	if (HAL_I2C_Init(&hi2c2) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN I2C2_Init 2 */

	/* USER CODE END I2C2_Init 2 */

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
	htim1.Init.Prescaler = 16 - 1;
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
 * @brief TIM2 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM2_Init(void) {

	/* USER CODE BEGIN TIM2_Init 0 */

	/* USER CODE END TIM2_Init 0 */

	TIM_ClockConfigTypeDef sClockSourceConfig = { 0 };
	TIM_MasterConfigTypeDef sMasterConfig = { 0 };

	/* USER CODE BEGIN TIM2_Init 1 */

	/* USER CODE END TIM2_Init 1 */
	htim2.Instance = TIM2;
	htim2.Init.Prescaler = 16 - 1;
	htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
	htim2.Init.Period = 999;
	htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
	if (HAL_TIM_Base_Init(&htim2) != HAL_OK) {
		Error_Handler();
	}
	sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
	if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK) {
		Error_Handler();
	}
	sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
	sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
	if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig)
			!= HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN TIM2_Init 2 */

	/* USER CODE END TIM2_Init 2 */

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
	htim3.Init.Prescaler = 16 - 1;
	htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
	htim3.Init.Period = 99;
	htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
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
 * @brief TIM14 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM14_Init(void) {

	/* USER CODE BEGIN TIM14_Init 0 */

	/* USER CODE END TIM14_Init 0 */

	TIM_OC_InitTypeDef sConfigOC = { 0 };

	/* USER CODE BEGIN TIM14_Init 1 */

	/* USER CODE END TIM14_Init 1 */
	htim14.Instance = TIM14;
	htim14.Init.Prescaler = 1;
	htim14.Init.CounterMode = TIM_COUNTERMODE_UP;
	htim14.Init.Period = 799;
	htim14.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	htim14.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
	if (HAL_TIM_Base_Init(&htim14) != HAL_OK) {
		Error_Handler();
	}
	if (HAL_TIM_PWM_Init(&htim14) != HAL_OK) {
		Error_Handler();
	}
	sConfigOC.OCMode = TIM_OCMODE_PWM1;
	sConfigOC.Pulse = 0;
	sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
	sConfigOC.OCFastMode = TIM_OCFAST_ENABLE;
	if (HAL_TIM_PWM_ConfigChannel(&htim14, &sConfigOC, TIM_CHANNEL_1)
			!= HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN TIM14_Init 2 */

	/* USER CODE END TIM14_Init 2 */
	HAL_TIM_MspPostInit(&htim14);

}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void) {
	GPIO_InitTypeDef GPIO_InitStruct = { 0 };

	/* GPIO Ports Clock Enable */
	__HAL_RCC_GPIOC_CLK_ENABLE();
	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOB_CLK_ENABLE();

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(GPIOC,
	GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_6 | GPIO_PIN_9,
			GPIO_PIN_RESET);

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2 | GPIO_PIN_4, GPIO_PIN_RESET);

	/*Configure GPIO pins : PC0 PC1 PC2 PC3
	 PC6 PC9 */
	GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3
			| GPIO_PIN_6 | GPIO_PIN_9;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

	/*Configure GPIO pins : PA2 PA4 */
	GPIO_InitStruct.Pin = GPIO_PIN_2 | GPIO_PIN_4;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	/*Configure GPIO pin : PA5 */
	GPIO_InitStruct.Pin = GPIO_PIN_5;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

}

/* USER CODE BEGIN 4 */
/* Human detection task with simple debouncing */
/* ---------------------- LCD TASK ------------------------ */
void lcdTask(void *pvParameters) {
	char line1[17], line2[17];

	while (1) {
		// Line 1: system status
		if (!pirAlert && accelAlert == 0 && !tempHumAlert && waterAlert == 0
				&& engineTempAlert == 0) {
			strcpy(line1, "NORMAL          ");
		} else {
			strcpy(line1, "ALERT!          ");
		}

		// Line 2: build compact detail string
		line2[0] = '\0';
		if (pirAlert)
			strncat(line2, "HUMAN ", 16 - strlen(line2));
		if (accelAlert == 2)
			strncat(line2, "VIB-CAUT ", 16 - strlen(line2));
		else if (accelAlert == 1)
			strncat(line2, "VIB-WARN ", 16 - strlen(line2));
		if (tempHumAlert)
			strncat(line2, "HEAT ", 16 - strlen(line2));
		if (waterAlert == 1)
			strncat(line2, "WATER-W ", 16 - strlen(line2));
		else if (waterAlert == 2)
			strncat(line2, "WATER-C ", 16 - strlen(line2));
		else if (waterAlert == 3)
			strncat(line2, "WATER! ", 16 - strlen(line2));

		if (engineTempAlert) {
			char tmp[9];
			snprintf(tmp, sizeof(tmp), "T:%2.0fC", engine_temperature);
			strncat(line2, tmp, 16 - strlen(line2));
		}

		if (line2[0] == '\0')
			strcpy(line2, "All OK         ");

		// Safe LCD update
		LCD_Update(0x80, line1);
		LCD_Update(0xC0, line2);

		vTaskDelay(pdMS_TO_TICKS(200));
	}
}

/* ---------------------- HUMAN DETECT TASK ------------------------ */
void humanDetect(void *pvParameters) {
	uint8_t lastState = 0;
	uint8_t stableCount = 0;

	while (1) {
		uint8_t currentState = (uint8_t) HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_5);
		if (currentState == lastState)
			stableCount++;
		else
			stableCount = 0;

		if (stableCount >= 7) {
			PIR_Status = currentState;
			pirAlert = PIR_Status;
		}

		lastState = currentState;
		vTaskDelay(pdMS_TO_TICKS(120));
	}
}

/* ---------------------- ACCIDENT DETECT TASK ------------------------ */
void accidentDetect(void *pvParameters) {
	MPU6050_Init();

	while (1) {
		MPU6050_Read_Accel();
		float a_total = sqrtf(ax * ax + ay * ay + az * az);

		if (a_total > 15.0f) {
			accelAlert = 2;
			HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6, GPIO_PIN_SET);
			HAL_GPIO_WritePin(GPIOC, GPIO_PIN_9, GPIO_PIN_SET);
		} else if (a_total > 12.0f) {
			accelAlert = 1;
			HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6, GPIO_PIN_SET);
			HAL_GPIO_WritePin(GPIOC, GPIO_PIN_9, GPIO_PIN_RESET);
		} else {
			accelAlert = 0;
			HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6, GPIO_PIN_RESET);
			HAL_GPIO_WritePin(GPIOC, GPIO_PIN_9, GPIO_PIN_RESET);
		}

		vTaskDelay(pdMS_TO_TICKS(120));
	}
}

void tempHum(void *pvParameters) {
	HAL_TIM_Base_Start(&htim1);
	DWT_Init();
	HAL_TIM_PWM_Start(&htim14, TIM_CHANNEL_1);
	Temp_motor_config();

	while (1) {
		Temperature_monitoring();
		vTaskDelay(pdMS_TO_TICKS(100));
	}
}

void waterDetect(void *pvParameters) {
	MotorB_Init();
	HAL_ADC_Start(&hadc1);
	HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);

	while (1) {
		Water_level_detection();
		vTaskDelay(pdMS_TO_TICKS(120));
	}
}

void engineTemp(void *pvParameters) {
	DWT_Delay_Init();
	DS18B20_GPIO_Init();
	while (1) {
		if (!DS18B20_Reset()) {
			//lprint(0x80, "No DS18B20");
			delay_ms(1000);
			continue;
		}

		// Start conversion and wait
		DS18B20_StartConversion();
		delay_ms(750);  // Important for stable readings

		raw = DS18B20_GetTemp();
		if (raw == -10000) {
			//lprint(0x80, "Sensor error!");
		} else {
			temp_int = raw / 16;
			temp_frac = ((raw % 16) * 625) / 100;

			float temperature = temp_int + (temp_frac / 100.0f);

			if (temperature > ENGINE_TEMP_THRESHOLD) {
				engineTempAlert = 1;
			} else {
				engineTempAlert = 0;
			}

		}
		vTaskDelay(pdMS_TO_TICKS(120));
	}
}

/* ---------------- MPU6050 ---------------- */
void MPU6050_Init(void) {
	uint8_t check = 0, data = 0;

	/* Use correct HAL I2C mem access size constant I2C_MEMADD_SIZE_8BIT */
	if (HAL_I2C_Mem_Read(&hi2c2, MPU6050_ADDR, WHO_AM_I_REG,
	I2C_MEMADD_SIZE_8BIT, &check, 1, HAL_MAX_DELAY) == HAL_OK) {
		if (check == 0x68) {
			data = 0;
			HAL_I2C_Mem_Write(&hi2c2, MPU6050_ADDR, PWR_MGMT_1,
			I2C_MEMADD_SIZE_8BIT, &data, 1, HAL_MAX_DELAY);
		}
	}
}

void MPU6050_Read_Accel(void) {
	uint8_t Rec_Data[6] = { 0 };

	/* Read 6 bytes starting at ACCEL_XOUT_H */
	if (HAL_I2C_Mem_Read(&hi2c2, MPU6050_ADDR, ACCEL_XOUT_H,
	I2C_MEMADD_SIZE_8BIT, Rec_Data, 6, HAL_MAX_DELAY) == HAL_OK) {
		accel_x = (int16_t) (Rec_Data[0] << 8 | Rec_Data[1]);
		accel_y = (int16_t) (Rec_Data[2] << 8 | Rec_Data[3]);
		accel_z = (int16_t) (Rec_Data[4] << 8 | Rec_Data[5]);

		/* convert to m/s^2 (assuming full scale ±2g, sensitivity 16384 LSB/g) */
		ax = (accel_x / 16384.0f) * 9.80665f;
		ay = (accel_y / 16384.0f) * 9.80665f;
		az = (accel_z / 16384.0f) * 9.80665f;
	} else {
		/* on I2C read failure, zero outputs (optional) */
		ax = ay = az = 0.0f;
	}
}
void Temp_motor_config() {
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET); //STANDBY 20
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_2, GPIO_PIN_SET); // AIN1 10
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_3, GPIO_PIN_RESET); // AIN2 11

	HAL_TIM_PWM_Start(&htim14, TIM_CHANNEL_1); //PA7 23

	__HAL_TIM_SET_COMPARE(&htim14, TIM_CHANNEL_1, 300);
}
void Temperature_monitoring(void) {
	if (DHT11_GetData(GPIOA, GPIO_PIN_2, &dht)) {

		if ((dht.Temperature >= 27 && dht.Humidity >= 73) && tempflag != 1) {
			tempHumAlert = 1;
			uint16_t pwm_values[] = { 100, 200, 300, 400 };
			for (int i = 0; i < 3; i++) {
				__HAL_TIM_SET_COMPARE(&htim14, TIM_CHANNEL_1, pwm_values[i]);
				vTaskDelay(pdMS_TO_TICKS(200));
			}
			tempflag = 1; // Motor is ON
		}

		else if ((dht.Temperature < 27 && dht.Humidity < 73) && tempflag == 1) {
			tempHumAlert = 0;
			uint16_t pwm_values[] = { 600, 400, 200, 0 };
			for (int i = 0; i < 4; i++) {
				__HAL_TIM_SET_COMPARE(&htim14, TIM_CHANNEL_1, pwm_values[i]);
				vTaskDelay(pdMS_TO_TICKS(200));
			}
			tempflag = 0; // Motor is OFF
		}

	}

}

void Water_level_detection(void) {
	static uint8_t state = 0;      // 0 = Normal, 3 = Water detected
	static TickType_t lastAlertTick = 0;

	// Start ADC and read value
	HAL_ADC_Start(&hadc1);
	HAL_ADC_PollForConversion(&hadc1, HAL_MAX_DELAY);
	adcValue = HAL_ADC_GetValue(&hadc1);

	if (adcValue < 1200) { // NORMAL
		if (state != 0) {
			// Water has returned to normal
			state = 0;
			waterAlert = 0;

			// Turn off LED/Buzzer
			HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6, GPIO_PIN_RESET);
			HAL_GPIO_WritePin(GPIOC, GPIO_PIN_9, GPIO_PIN_RESET);

			// Resume motor at normal speed (running condition)
			__HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 50);
		}
	} else if (adcValue >= 1500 && adcValue <= 3500) {
		waterAlert = 1;
		state = 1;
		HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6, GPIO_PIN_SET);
	} else { // WATER DETECTED
		state = 2;           // Critical
		waterAlert = 2;

		// Turn ON LED
		HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6, GPIO_PIN_SET);

		// Optional: Buzzer ON intermittently every 500 ms
		TickType_t now = xTaskGetTickCount();
		if ((now - lastAlertTick) >= pdMS_TO_TICKS(500)) {
			HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_9); // toggle buzzer
			lastAlertTick = now;
		}

		// STOP motor immediately
		__HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 0);
	}
}

void Set_Temp_Motor_Speed(uint8_t dutyCycle) {
	__HAL_TIM_SET_COMPARE(&htim14, TIM_CHANNEL_1,
			(htim14.Init.Period * dutyCycle) / 100);
}

/* ================= Motor B Initialization ================= */
void MotorB_Init(void) {
	// Enable motor driver standby (STBY high)
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);

	// Set Motor B forward (BIN1=1, BIN2=0)
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_0, GPIO_PIN_SET);   // BIN1
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_1, GPIO_PIN_RESET); // BIN2

	// Start PWM output on TIM3 CH1 (PA6)
	HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);

	// Motor starts in running condition at medium speed
	__HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 50);
}

/* Called by FreeRTOS for runtime stats (FreeRTOS expects this symbol) */
void ConfigureRunTimeStatsTimer(void) {
	/* Start TIM2 with interrupt; timer is configured to tick at ~1 kHz (1 ms) */
	HAL_TIM_Base_Start_IT(&htim2);
}
unsigned long ulGetRunTimeCounterValue(void) {
	return runtimeStatsTimer; // returns current timer count
}

/* LCD update helper that keeps last text per line and avoids redundant writes */
void LCD_Update(uint8_t addr, char *text) {
	if (xSemaphoreTake(lcdMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
		lprint(addr, text);
		xSemaphoreGive(lcdMutex);
	}
}

/* USER CODE END 4 */

/**
 * @brief  Period elapsed callback in non blocking mode
 * @note   This function is called  when TIM6 interrupt took place, inside
 * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
 * a global variable "uwTick" used as application time base.
 * @param  htim : TIM handle
 * @retval None
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
	if (htim->Instance == TIM6) {
		HAL_IncTick();
	} else if (htim->Instance == TIM2) {
		runtimeStatsTimer++;
	}
}

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void) {
	/* USER CODE BEGIN Error_Handler_Debug */
	/* User can add his own implementation to report the HAL error return state */
	__disable_irq();
	while (1) {
	}
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
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

