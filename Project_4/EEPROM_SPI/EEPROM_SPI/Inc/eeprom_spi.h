/*
 * eeprom_spi.h
 *
 *  Created on: Sep 9, 2025
 *      Author: WZS1KOR
 */

#ifndef EEPROM_SPI_H_
#define EEPROM_SPI_H_


#include "stm32f405xx.h"

#define EEPROM_SPI         SPI1
#define EEPROM_CS_LOW()    (GPIOA->BSRR = (1 << (4 + 16)))  // Reset PA4
#define EEPROM_CS_HIGH()   (GPIOA->BSRR = (1 << 4))         // Set PA4

/* EEPROM Commands */
#define EEPROM_WREN        0x06
#define EEPROM_WRDI        0x04
#define EEPROM_RDSR        0x05
#define EEPROM_WRSR        0x01
#define EEPROM_READ        0x03
#define EEPROM_WRITE       0x02

void SPI1_Init(void);
uint8_t SPI1_Transmit(uint8_t data);
void EEPROM_WriteEnable(void);
void EEPROM_WriteByte(uint16_t addr, uint8_t data);
uint8_t EEPROM_ReadByte(uint16_t addr);



#endif /* EEPROM_SPI_H_ */
