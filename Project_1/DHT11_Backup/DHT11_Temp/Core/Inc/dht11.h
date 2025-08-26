/*
 * dht11.h
 *
 *  Created on: Jun 27, 2025
 *      Author: WZS1KOR
 */

#ifndef INC_DHT11_H_
#define INC_DHT11_H_

#include "stm32f4xx_hal.h"

typedef struct {
    uint8_t Temperature;
    uint8_t Humidity;
} DHT11_Data;

void DWT_Init(void);
void delay_us(uint32_t us);
uint8_t DHT11_GetData(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin, DHT11_Data *data);



#endif /* INC_DHT11_H_ */
