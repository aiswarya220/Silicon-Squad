/*
 * eeprom_spi.c
 *
 *  Created on: Sep 9, 2025
 *      Author: WZS1KOR
 */


#include "eeprom_spi.h"

void SPI1_Init(void)
{
    // Enable SPI1 and GPIOA clock
    RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;

    // PA5 = SCK, PA6 = MISO, PA7 = MOSI -> AF5
    GPIOA->MODER &= ~((3U << (5*2)) | (3U << (6*2)) | (3U << (7*2)));
    GPIOA->MODER |=  (2U << (5*2)) | (2U << (6*2)) | (2U << (7*2));
    GPIOA->AFR[0] |= (5U << (5*4)) | (5U << (6*4)) | (5U << (7*4));

    // PA4 = CS -> Output
    GPIOA->MODER &= ~(3U << (4*2));
    GPIOA->MODER |=  (1U << (4*2));
    EEPROM_CS_HIGH();

    // SPI1 Config
    EEPROM_SPI->CR1 = SPI_CR1_MSTR;        // Master mode
    EEPROM_SPI->CR1 |= SPI_CR1_BR_1 | SPI_CR1_BR_0; // Baud rate = Fpclk/16 (~2.6MHz)
    EEPROM_SPI->CR1 |= SPI_CR1_SSI | SPI_CR1_SSM;   // Software slave select
    EEPROM_SPI->CR1 &= ~(SPI_CR1_CPOL | SPI_CR1_CPHA); // Mode 0
    EEPROM_SPI->CR1 |= SPI_CR1_SPE;       // Enable SPI
}

uint8_t SPI1_Transmit(uint8_t data)
{
    while(!(EEPROM_SPI->SR & SPI_SR_TXE));  // Wait for TX ready
    *((__IO uint8_t *)&EEPROM_SPI->DR) = data;
    while(!(EEPROM_SPI->SR & SPI_SR_RXNE)); // Wait for RX ready
    return *((__IO uint8_t *)&EEPROM_SPI->DR);
}

void EEPROM_WriteEnable(void)
{
    EEPROM_CS_LOW();
    SPI1_Transmit(EEPROM_WREN);
    EEPROM_CS_HIGH();
}

void EEPROM_WriteByte(uint16_t addr, uint8_t data)
{
    EEPROM_WriteEnable();

    EEPROM_CS_LOW();
    SPI1_Transmit(EEPROM_WRITE);
    SPI1_Transmit((addr >> 8) & 0xFF);
    SPI1_Transmit(addr & 0xFF);
    SPI1_Transmit(data);
    EEPROM_CS_HIGH();

    // Wait until write completes
    EEPROM_CS_LOW();
    SPI1_Transmit(EEPROM_RDSR);
    while(SPI1_Transmit(0xFF) & 0x01);
    EEPROM_CS_HIGH();
}

uint8_t EEPROM_ReadByte(uint16_t addr)
{
    uint8_t data;

    EEPROM_CS_LOW();
    SPI1_Transmit(EEPROM_READ);
    SPI1_Transmit((addr >> 8) & 0xFF);
    SPI1_Transmit(addr & 0xFF);
    data = SPI1_Transmit(0xFF);
    EEPROM_CS_HIGH();

    return data;
}
