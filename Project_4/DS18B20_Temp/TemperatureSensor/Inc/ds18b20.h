#ifndef DS18B20_H
#define DS18B20_H

#include "stm32f405xx.h"
#include "system_stm32f4xx.h"
#include <stdint.h>

// ================= Configuration =================
#define DS18B20_PORT GPIOA
#define DS18B20_PIN  6   // <-- Change this based on your wiring

// =============== Function Prototypes ===============
void DS18B20_GPIO_Init(void);
uint8_t DS18B20_Reset(void);
void DS18B20_WriteByte(uint8_t data);
uint8_t DS18B20_ReadByte(void);
int16_t DS18B20_GetTemp(void);

void DWT_Delay_Init(void);
void delay_us(uint32_t us);
void delay_ms(uint32_t ms);
void DS18B20_StartConversion(void);

#endif
