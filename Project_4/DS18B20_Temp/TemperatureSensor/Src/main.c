#include "ds18b20.h"
#include "lcd.h"
#include <stdio.h>
#include <stdint.h>

int16_t raw = 0;
int16_t temp_int = 0;
int16_t temp_frac = 0;
char buff[20] = { 0 };

int main(void) {
	DWT_Delay_Init();
	DS18B20_GPIO_Init();
	LcdInit();

	while (1) {
		if (!DS18B20_Reset()) {
			lprint(0x80, "No DS18B20");
			delay_ms(1000);
			continue;
		}

		// Start conversion and wait
		DS18B20_StartConversion();
		delay_ms(750);  // Important for stable readings

		raw = DS18B20_GetTemp();
		if (raw == -10000) {
			lprint(0x80, "Sensor error!");
		} else {
			temp_int = raw / 16;
			temp_frac = ((raw % 16) * 625) / 100;

			snprintf(buff, sizeof(buff), "T:%d.%02d%cC", temp_int, temp_frac,
					0xDF);
			lprint(0x80, buff);
		}

		delay_ms(1000);
	}
}
