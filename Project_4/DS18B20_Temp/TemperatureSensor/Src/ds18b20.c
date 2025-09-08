#include "ds18b20.h"
#define SKIP_ROM         0xCC
#define CONVERT_T_CMD    0x44
// =============== Macros for Pin Control ===============
#define DS_LOW()    (DS18B20_PORT->BSRR = (1U << (DS18B20_PIN + 16)))
#define DS_HIGH()   (DS18B20_PORT->BSRR = (1U << DS18B20_PIN))
#define DS_READ()   ((DS18B20_PORT->IDR >> DS18B20_PIN) & 1U)

// =======================================================
//               Delay Functions Using DWT
// =======================================================
void DWT_Delay_Init(void) {
	CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk; // Enable TRC
	DWT->CYCCNT = 0;                               // Reset counter
	DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;            // Enable counter
}

void delay_us(uint32_t us) {
	uint32_t start = DWT->CYCCNT;
	uint32_t ticks = us * (SystemCoreClock / 1000000);
	while ((DWT->CYCCNT - start) < ticks)
		;
}

void delay_ms(uint32_t ms) {
	while (ms--)
		delay_us(1000);
}

// =======================================================
//               GPIO Initialization
// =======================================================
void DS18B20_GPIO_Init(void) {
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;

	// Set pin as output + open-drain + pull-up
	DS18B20_PORT->MODER &= ~(3U << (DS18B20_PIN * 2));
	DS18B20_PORT->MODER |= (1U << (DS18B20_PIN * 2));   // Output mode
	DS18B20_PORT->OTYPER |= (1U << DS18B20_PIN);         // Open-drain
	DS18B20_PORT->PUPDR &= ~(3U << (DS18B20_PIN * 2));
	DS18B20_PORT->PUPDR |= (1U << (DS18B20_PIN * 2));  // Pull-up enabled
	DS_HIGH();
}

// =======================================================
//               DS18B20 Reset and Presence Detect
// =======================================================
uint8_t DS18B20_Reset(void) {
	uint8_t presence = 0;
	DS_LOW();
	delay_us(480);
	DS_HIGH();
	delay_us(70);
	presence = !DS_READ();   // Sensor pulls low if present
	delay_us(410);
	return presence;
}

// =======================================================
//               Write 1 Byte to DS18B20
// =======================================================
void DS18B20_WriteByte(uint8_t data) {
	for (uint8_t i = 0; i < 8; i++) {
		DS_LOW();
		if (data & (1 << i)) {
			delay_us(5);     // Short low
			DS_HIGH();
			delay_us(60);
		} else {
			delay_us(60);    // Keep low longer
			DS_HIGH();
			delay_us(5);
		}
	}
}

// =======================================================
//               Read 1 Byte from DS18B20
// =======================================================
uint8_t DS18B20_ReadByte(void) {
	uint8_t value = 0;
	for (uint8_t i = 0; i < 8; i++) {
		DS_LOW();
		delay_us(2);
		DS_HIGH();
		delay_us(15); // <-- Corrected timing
		if (DS_READ())
			value |= (1 << i);
		delay_us(45);
	}
	return value;
}

// =======================================================
//               Get Temperature (Raw + Celsius)
// =======================================================
int16_t DS18B20_GetTemp(void) {
	uint8_t lsb, msb;

	if (!DS18B20_Reset())
		return -10000; // No sensor found

	DS18B20_WriteByte(0xCC); // Skip ROM
	DS18B20_WriteByte(0x44); // Start temperature conversion
	delay_ms(750);          // Max conversion time

	if (!DS18B20_Reset())
		return -10000;

	DS18B20_WriteByte(0xCC); // Skip ROM
	DS18B20_WriteByte(0xBE); // Read scratchpad

	lsb = DS18B20_ReadByte();
	msb = DS18B20_ReadByte();

	return (msb << 8) | lsb; // Raw temperature value
}
void DS18B20_StartConversion(void) {
	// Reset the sensor
	if (!DS18B20_Reset())
		return;

	// Skip ROM (single sensor)
	DS18B20_WriteByte(SKIP_ROM);

	// Start temperature conversion
	DS18B20_WriteByte(CONVERT_T_CMD);
}
