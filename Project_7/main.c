/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Vehicle Time Management System with UART RS232
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2025 STMicroelectronics.
 * All rights reserved.
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "lcd.h"
#include "ds1302.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef enum {
	VIEW_MODE = 0, EDIT_MODE = 1
} ui_mode_t;

typedef enum {
	FIELD_HOUR = 0, FIELD_MINUTE, FIELD_AMPM, FIELD_COUNT
} TimeField;

typedef enum {
	DATE_FIELD_DAY = 0,
	DATE_FIELD_MONTH,
	DATE_FIELD_YEAR,
	DATE_FIELD_WDAY,
	DATE_FIELD_COUNT
} DateField;

static TimeField curField = FIELD_HOUR;
static DateField curDateField = DATE_FIELD_DAY;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define SW1_GPIO_Port GPIOB
#define SW1_Pin       GPIO_PIN_7
#define SW2_GPIO_Port GPIOB
#define SW2_Pin       GPIO_PIN_3
#define SW3_GPIO_Port GPIOB
#define SW3_Pin       GPIO_PIN_4
#define SW4_GPIO_Port GPIOA
#define SW4_Pin       GPIO_PIN_15

/* Debounce */
#define DEBOUNCE_MS   35u

/* Set to 1 to initialize RTC with default time on startup */
#define INIT_RTC_ON_STARTUP  1

typedef struct {
	uint8_t day;    // 1..31
	uint8_t month;  // 1..12
	uint16_t year;  // 2000..2099
	uint8_t wday;   // 1..7
	uint8_t hour;   // 0..23 internally
	uint8_t minute; // 0..59
	uint8_t use12h; // 0=24h, 1=12h
	uint8_t isPM;   // valid only when use12h=1
} datetime_t;

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart3;

/* USER CODE BEGIN PV */
uint8_t hr, min, sec;
char timeStr[20];

static ui_mode_t g_mode = VIEW_MODE;
static datetime_t g_dt;
static datetime_t g_dt_last; // For flicker prevention

/* Cursor position within our edit schema */
static uint8_t curRow = 0; // 0 = top row (date), 1 = bottom row (time)
static uint8_t curCol = 0; // logical column index within that row

/* UART time logging */
static uint8_t last_logged_min = 255;

typedef struct {
	uint8_t last_state;
	uint32_t last_time;
} btn_state_t;

static btn_state_t b1 = { 1, 0 }, b2 = { 1, 0 }, b3 = { 1, 0 }, b4 = { 1, 0 };
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART3_UART_Init(void);
/* USER CODE BEGIN PFP */
/* DS1302 helpers for full date/time */
static void DS1302_GetDateTime(datetime_t *out);
static void DS1302_SetDateTime(const datetime_t *in);
static void DS1302_InitWithDefaultTime(void);

/* UI / LCD helpers */
void dt_from_rtc_encoding(datetime_t *io);
void dt_to_rtc_encoding(datetime_t *io);
static void render_view(const datetime_t *dt);
static void render_edit(const datetime_t *dt);
static void apply_bounds(datetime_t *dt);
static void to_12h(const datetime_t *in24, uint8_t *h12, uint8_t *isPM);
static uint8_t days_in_month(uint8_t m, uint16_t y);
static const char* wday_name(uint8_t w);
static uint8_t datetime_changed(const datetime_t *a, const datetime_t *b);
static void check_manual_lcd_reset(void);

/* UART helpers */
static void log_time_to_uart(const datetime_t *dt);
static void UART_SendString(const char *str);

/* Buttons */
uint8_t btn_falling(GPIO_TypeDef *port, uint16_t pin, btn_state_t *b);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

void LCD_ShowCursor(uint8_t visible, uint8_t blink) {
	uint8_t cmd = 0x0C; // display on, cursor off, blink off
	if (visible)
		cmd = 0x0E;      // cursor ON, no blink
	if (visible && blink)
		cmd = 0x0F; // cursor + blink
	LcdFxn(0, cmd);
}

void LCD_SetCursor(uint8_t row, uint8_t col) {
	uint8_t addr = (row == 0 ? 0x80 : 0xC0) + col;
	LcdFxn(0, addr);
}

uint8_t btn_falling(GPIO_TypeDef *port, uint16_t pin, btn_state_t *b) {
	uint8_t raw = HAL_GPIO_ReadPin(port, pin);
	uint32_t now = HAL_GetTick();
	uint8_t falling = 0;

	if (raw != b->last_state && (now - b->last_time) > DEBOUNCE_MS) {
		b->last_time = now;
		if (b->last_state == 1 && raw == 0)
			falling = 1;  // high → low transition
		b->last_state = raw;
	}
	return falling;
}

static uint8_t days_in_month(uint8_t m, uint16_t y) {
	static const uint8_t dpm[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30,
			31 };
	if (m == 0 || m > 12)
		return 31;
	uint8_t d = dpm[m - 1];
	if (m == 2) {
		uint8_t leap = ((y % 4 == 0 && y % 100 != 0) || (y % 400 == 0));
		if (leap)
			d = 29;
	}
	return d;
}

static const char* wday_name(uint8_t w) {
	static const char *names[7] = { "MON", "TUE", "WED", "THU", "FRI", "SAT",
			"SUN" };
	if (w < 1 || w > 7)
		return "MON";
	return names[w - 1];
}

static void to_12h(const datetime_t *in24, uint8_t *h12, uint8_t *isPM) {
	uint8_t h = in24->hour;
	*isPM = (h >= 12) ? 1 : 0;
	uint8_t hh = h % 12;
	if (hh == 0)
		hh = 12;
	*h12 = hh;
}

/* Check if datetime changed (for flicker prevention) */
static uint8_t datetime_changed(const datetime_t *a, const datetime_t *b) {
	return (a->day != b->day || a->month != b->month || a->year != b->year ||
			a->wday != b->wday || a->hour != b->hour || a->minute != b->minute ||
			a->isPM != b->isPM);
}

/* Manual LCD reset: Hold SW1+SW2 together for 1 second */
static void check_manual_lcd_reset(void) {
	static uint32_t both_pressed_time = 0;

	// Only in VIEW mode to avoid interfering with editing
	if (g_mode == VIEW_MODE &&
	    HAL_GPIO_ReadPin(SW1_GPIO_Port, SW1_Pin) == 0 &&
	    HAL_GPIO_ReadPin(SW2_GPIO_Port, SW2_Pin) == 0) {

		if (both_pressed_time == 0) {
			both_pressed_time = HAL_GetTick();
		} else if (HAL_GetTick() - both_pressed_time > 1000) {
			// Force LCD re-init
			LcdInit();
			HAL_Delay(50);
			render_view(&g_dt);
			both_pressed_time = 0;

			// Confirm via UART
			UART_SendString("LCD Reset!\r\n");
		}
	} else {
		both_pressed_time = 0;
	}
}

/* UART Helper: Send string via RS232 */
static void UART_SendString(const char *str) {
	HAL_UART_Transmit(&huart3, (uint8_t*)str, strlen(str), 100);
}

/* Log time to UART every minute */
static void log_time_to_uart(const datetime_t *dt) {
	// Only log when minute changes
	if (dt->minute == last_logged_min)
		return;

	last_logged_min = dt->minute;

	char buffer[80];

	// Send date line
	snprintf(buffer, sizeof(buffer), "D: %02u-%02u-%04u, %s\r\n",
	         dt->day, dt->month, dt->year, wday_name(dt->wday));
	UART_SendString(buffer);

	// Send time line based on format
	if (dt->use12h) {
		snprintf(buffer, sizeof(buffer), "T: %02u:%02u %s\r\n",
		         dt->hour, dt->minute, dt->isPM ? "PM" : "AM");
	} else {
		snprintf(buffer, sizeof(buffer), "T: %02u:%02u\r\n",
		         dt->hour, dt->minute);
	}
	UART_SendString(buffer);

	// Send separator
	UART_SendString("---\r\n");
}

/* DS1302 register map (write even, read odd):
 0x80/0x81 sec (bit7 = CH), 0x82/0x83 min, 0x84/0x85 hr,
 0x86/0x87 date(1..31), 0x88/0x89 month(1..12),
 0x8A/0x8B day(1..7), 0x8C/0x8D year(00..99) */

static void DS1302_GetDateTime(datetime_t *out) {
	uint8_t sec = DS1302_Read(0x81) & 0x7F;
	uint8_t min = DS1302_Read(0x83) & 0x7F;
	uint8_t hour = DS1302_Read(0x85);       // we'll force 24h
	uint8_t date = DS1302_Read(0x87) & 0x3F;
	uint8_t mon = DS1302_Read(0x89) & 0x1F;
	uint8_t wday = DS1302_Read(0x8B) & 0x07;
	uint8_t year = DS1302_Read(0x8D);       // 00..99

	// BCD → binary helper
	uint8_t bcd_to_bin(uint8_t x) {
		return (uint8_t) (((x >> 4) * 10) + (x & 0x0F));
	}

	out->minute = bcd_to_bin(min);
	out->hour = bcd_to_bin(hour & 0x3F);     // FIXED: Now properly converts BCD to binary
	out->day = bcd_to_bin(date);
	out->month = bcd_to_bin(mon);
	out->year = 2000 + bcd_to_bin(year);
	out->wday = (wday >= 1 && wday <= 7) ? wday : 3; // default WED
}

static void DS1302_SetDateTime(const datetime_t *in) {
	datetime_t tmp = *in;

	// We store hours to DS1302 in 24h always (cleanest)
	if (tmp.use12h) {
		// Convert 12h→24h for storage
		uint8_t h12 = tmp.hour; // caller passes 1..12 in edit when use12h=1
		uint8_t h24 = (h12 % 12);
		if (tmp.isPM)
			h24 += 12;
		if (h24 == 24)
			h24 = 12; // just in case
		tmp.hour = h24;
	}

	// Bounds/clamp before write
	apply_bounds(&tmp);

	// Binary → BCD helper
	uint8_t bin_to_bcd(uint8_t x) {
		return (uint8_t) (((x / 10) << 4) | (x % 10));
	}

	uint8_t b_sec = 0x00; // write 0 seconds & CH=0 (or preserve seconds if desired)
	uint8_t b_min = bin_to_bcd(tmp.minute);
	uint8_t b_hr = bin_to_bcd(tmp.hour & 0x3F); // FIXED: Now properly converts binary to BCD
	uint8_t b_date = bin_to_bcd(tmp.day);
	uint8_t b_mon = bin_to_bcd(tmp.month);
	uint8_t b_wday = (tmp.wday >= 1 && tmp.wday <= 7) ? tmp.wday : 3;
	uint8_t b_year = bin_to_bcd((uint8_t) (tmp.year % 100));

	DS1302_Write(0x8E, 0x00);   // disable WP
	// Keep current seconds; only ensure CH=0
	uint8_t curS = DS1302_Read(0x81) & 0x7F;
	DS1302_Write(0x80, curS);
	DS1302_Write(0x82, b_min);
	DS1302_Write(0x84, b_hr);
	DS1302_Write(0x86, b_date);
	DS1302_Write(0x88, b_mon);
	DS1302_Write(0x8A, b_wday);
	DS1302_Write(0x8C, b_year);
	DS1302_Write(0x8E, 0x80);   // enable WP
}

/* Initialize RTC with default time: 12:39 PM */
static void DS1302_InitWithDefaultTime(void) {
	datetime_t init_time;

	// ===== INITIAL TIME: 12:39 PM =====
	init_time.day = 15;           // Day: 15
	init_time.month = 10;         // Month: October
	init_time.year = 2025;        // Year: 2025
	init_time.wday = 3;           // Weekday: 3 = Wednesday
	init_time.hour = 17;          // Hour: 12 (12 PM in 24h format)
	init_time.minute = 39;        // Minute: 39
	init_time.use12h = 1;         // Use 24h format for initialization
	init_time.isPM = 0;           // Not used when use12h = 0
	// ==================================

	// Write to RTC
	DS1302_SetDateTime(&init_time);

	// Brief delay to ensure write completes
	HAL_Delay(10);
}

/* Enforce valid ranges */
static void apply_bounds(datetime_t *dt) {
	if (dt->month < 1)
		dt->month = 1;
	if (dt->month > 12)
		dt->month = 12;
	uint8_t dim = days_in_month(dt->month, dt->year);
	if (dt->day < 1)
		dt->day = 1;
	if (dt->day > dim)
		dt->day = dim;
	if (dt->minute > 59)
		dt->minute = 59;

	if (dt->use12h) {
		if (dt->hour < 1)
			dt->hour = 12;
		if (dt->hour > 12)
			dt->hour = 12;
		dt->isPM = dt->isPM ? 1 : 0;
	} else {
		if (dt->hour > 23)
			dt->hour = 23;
	}
	if (dt->wday < 1 || dt->wday > 7)
		dt->wday = 3; // WED default
}

/* Renderers */
static void render_view(const datetime_t *dt) {
	char l1[17], l2[17];
	snprintf(l1, sizeof l1, "D:%02u-%02u-%04u,%s", dt->day, dt->month, dt->year,
			wday_name(dt->wday));

	if (dt->use12h) {
		// When use12h is true, hour is already 1-12 and isPM is already set
		snprintf(l2, sizeof l2, "T:%02u:%02u %s", dt->hour, dt->minute,
				dt->isPM ? "PM" : "AM");
	} else {
		snprintf(l2, sizeof l2, "T:%02u:%02u", dt->hour, dt->minute);
	}

	// Print without clearing the whole display each frame
	lprint(0x80, l1);
	lprint(0xC0, l2);
}

static void render_edit(const datetime_t *dt) {
	// 1. Always start by drawing the latest values
	render_view(dt);

	// 2. Enable the cursor with blinking (LCD command 0x0F)
	LCD_ShowCursor(1, 1);

	// 3. Move the cursor based on what we are editing
	if (curRow == 0) {
		// ---- DATE ROW ----
		// Field-based cursor placement for date
		switch (curDateField) {
		case DATE_FIELD_DAY:
			LCD_SetCursor(0, 2);  // First digit of day
			break;
		case DATE_FIELD_MONTH:
			LCD_SetCursor(0, 5);  // First digit of month
			break;
		case DATE_FIELD_YEAR:
			LCD_SetCursor(0, 8);  // First digit of year
			break;
		case DATE_FIELD_WDAY:
			LCD_SetCursor(0, 13); // First letter of weekday
			break;
		default:
			LCD_SetCursor(0, 2);
			break;
		}
	} else {
		// ---- TIME ROW ----
		// Field-based cursor placement: hour → minute → AM/PM
		switch (curField) {
		case FIELD_HOUR:
			// Move cursor to the first digit of hour ("1" of "12")
			LCD_SetCursor(1, 2);
			break;
		case FIELD_MINUTE:
			// Move cursor to the first digit of minutes ("3" of "39")
			LCD_SetCursor(1, 5);
			break;
		case FIELD_AMPM:
			// Move cursor to the 'P' of "PM" (or 'A' of "AM")
			if (dt->use12h)
				LCD_SetCursor(1, 8);
			else
				LCD_SetCursor(1, 2); // fallback for 24h mode
			break;
		default:
			LCD_SetCursor(1, 2);
			break;
		}
	}
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
  MX_USART3_UART_Init();
  /* USER CODE BEGIN 2 */
	LcdInit();
	HAL_Delay(100);
	DS1302_Write(0x8E, 0x00);  // Disable write protect
	uint8_t s = DS1302_Read(0x81) & 0x7F;  // Clear CH bit
	DS1302_Write(0x80, s);
	DS1302_Write(0x8E, 0x80);  // Enable write protect

#if INIT_RTC_ON_STARTUP
	// ===== INITIALIZE RTC WITH DEFAULT TIME: 12:39 PM =====
	DS1302_InitWithDefaultTime();
#endif

	/* Startup: load RTC → g_dt; default to 12h display */
	g_dt.use12h = 1;  // default to 12h UI
	DS1302_GetDateTime(&g_dt);

	// Convert to 12h if needed
	if (g_dt.use12h) {
		uint8_t h12, pm;
		to_12h(&g_dt, &h12, &pm);
		g_dt.hour = h12;
		g_dt.isPM = pm;
	}

	/* Draw initial display */
	render_view(&g_dt);
	g_dt_last = g_dt; // Initialize last state

	// Send startup message via UART
	UART_SendString("\r\n=== RTC System Started ===\r\n");

	uint32_t last_tick = HAL_GetTick();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
	while (1) {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
		// Check for manual LCD reset (Hold SW1+SW2 for 1 second)
		check_manual_lcd_reset();

		if (g_mode == VIEW_MODE) {
			if (HAL_GetTick() - last_tick >= 1000) { // Check every 1 second
				last_tick = HAL_GetTick();
				datetime_t now;
				DS1302_GetDateTime(&now);
				now.use12h = g_dt.use12h; // persist user display pref

				// Convert to 12h format if needed for editing consistency
				if (now.use12h) {
					uint8_t h12, pm;
					to_12h(&now, &h12, &pm);
					now.hour = h12;  // Store as 1-12 when use12h is active
					now.isPM = pm;   // Properly set AM/PM flag
				}

				// ANTI-FLICKER: Only update LCD if time actually changed
				if (datetime_changed(&now, &g_dt_last)) {
					render_view(&now);
					g_dt_last = now;
				}

				// Send time to UART every minute (non-blocking)
				log_time_to_uart(&now);

				g_dt = now; // keep copy
			}

			if (btn_falling(SW1_GPIO_Port, SW1_Pin, &b1)) {
				g_mode = EDIT_MODE;
				curRow = 0;
				curDateField = DATE_FIELD_DAY;
				curField = FIELD_HOUR;
				render_edit(&g_dt);
				HAL_Delay(150);
			}
		}

		/* --- EDIT mode: follow the spec with 4 switches --- */
		if (g_mode == EDIT_MODE) {
			/* Each switch works independently (not nested). */

			// SW1: increment current digit/letter / field
			if (btn_falling(SW1_GPIO_Port, SW1_Pin, &b1)) {
				if (curRow == 0) {
					// Row 1: date field-based editing
					switch (curDateField) {
					case DATE_FIELD_DAY:
						g_dt.day = (g_dt.day % days_in_month(g_dt.month, g_dt.year)) + 1;
						break;
					case DATE_FIELD_MONTH:
						g_dt.month = (g_dt.month % 12) + 1;
						break;
					case DATE_FIELD_YEAR:
						g_dt.year++;
						if (g_dt.year > 2099)
							g_dt.year = 2000;
						break;
					case DATE_FIELD_WDAY:
						g_dt.wday = (g_dt.wday % 7) + 1;
						break;
					default:
						break;
					}
					apply_bounds(&g_dt);
					render_edit(&g_dt);
				} else {
					// Row 2: time editing
					switch (curField) {
					case FIELD_HOUR:
						if (g_dt.use12h) {
							g_dt.hour = (g_dt.hour % 12) + 1;
						} else {
							g_dt.hour = (g_dt.hour + 1) % 24;
						}
						break;
					case FIELD_MINUTE:
						g_dt.minute = (g_dt.minute + 1) % 60;
						break;
					case FIELD_AMPM:
						if (g_dt.use12h)
							g_dt.isPM = !g_dt.isPM;
						break;
					default:
						break;
					}
					apply_bounds(&g_dt);
					render_edit(&g_dt);
				}
			}

			// SW2: confirm → next field (wrap)
			if (btn_falling(SW2_GPIO_Port, SW2_Pin, &b2)) {
				if (curRow == 1) {
					// Move to next time field
					curField++;
					if (!g_dt.use12h && curField == FIELD_AMPM)
						curField = FIELD_HOUR;  // skip AM/PM in 24h
					if (curField >= FIELD_COUNT)
						curField = FIELD_HOUR;
				} else {
					// Move to next date field
					curDateField++;
					if (curDateField >= DATE_FIELD_COUNT)
						curDateField = DATE_FIELD_DAY;
				}
				render_edit(&g_dt);
			}

			// SW3: move cursor to next row, same column (wrap)
			if (btn_falling(SW3_GPIO_Port, SW3_Pin, &b3)) {
				curRow = (curRow == 0) ? 1 : 0;
				// when switching to time row, ensure curField valid
				if (curRow == 1) {
					curField = FIELD_HOUR;
				} else {
					curDateField = DATE_FIELD_DAY;
				}
				render_edit(&g_dt);
			}

			// SW4: write to RTC and exit edit mode
			if (btn_falling(SW4_GPIO_Port, SW4_Pin, &b4)) {
				// Convert 12h editing hour to storage if needed happens inside SetDateTime
				DS1302_SetDateTime(&g_dt);
				g_mode = VIEW_MODE;
				LCD_ShowCursor(0, 0); // hide cursor on exit
				render_view(&g_dt);
				g_dt_last = g_dt; // Update last state

				// Send confirmation via UART
				UART_SendString("Time Updated!\r\n");
				log_time_to_uart(&g_dt);

				HAL_Delay(150);
			}
		}
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
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief USART3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART3_UART_Init(void)
{

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
  if (HAL_UART_Init(&huart3) != HAL_OK)
  {
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
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_0, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10, GPIO_PIN_RESET);

  /*Configure GPIO pin : PC0 */
  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : PB10 */
  GPIO_InitStruct.Pin = GPIO_PIN_10;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : PB11 PB3 PB4 PB7 */
  GPIO_InitStruct.Pin = GPIO_PIN_11|GPIO_PIN_3|GPIO_PIN_4|GPIO_PIN_7;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : PA15 */
  GPIO_InitStruct.Pin = GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM2 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM2) {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
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
