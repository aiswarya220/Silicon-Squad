#include "stm32f405xx.h"
#include <stdint.h>
#include <stdio.h>
#include "lcd.h"
#include "eeprom_spi.h"

#define BUTTON_PA15 (1<<15)
#define EEPROM_RECORD_SIZE 2   // 2 bytes for counter

uint16_t eeprom_addr = 0;
uint16_t counter = 0;

// ---------------- GPIOA PA15 input ----------------
void GPIOA15_Init(void) {
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;     // Enable GPIOA clock
    GPIOA->MODER &= ~(3 << (15*2));          // Input mode
    GPIOA->PUPDR &= ~(3 << (15*2));
    GPIOA->PUPDR |=  (1 << (15*2));          // Pull-up
}

// ---------------- Simple delay ----------------
void delay_ms(uint32_t ms) {
    for(uint32_t i=0; i<ms*16000; i++) __NOP();
}

// ---------------- Simple SysTick for 1ms tick ----------------
volatile uint32_t msTicks = 0;
void SysTick_Handler(void) {
    msTicks++;
}
uint32_t HAL_GetTick(void) {
    return msTicks;
}

// ---------------- Main ----------------
int main(void) {
    // Initialize SysTick for 1ms tick
    SystemCoreClockUpdate();
    SysTick_Config(SystemCoreClock / 1000);

    // Initialize SPI1 for EEPROM and LCD
    SPI1_Init();
    LcdInit();
    GPIOA15_Init();

    uint32_t last_tick = 0;

    while(1) {
        // ---------------- Increment counter every 1 second ----------------
        if (HAL_GetTick() - last_tick >= 1000) {
            counter++;

            // Store counter in EEPROM (big-endian)
            EEPROM_WriteByte(eeprom_addr, (counter >> 8) & 0xFF);
            EEPROM_WriteByte(eeprom_addr + 1, counter & 0xFF);

            last_tick = HAL_GetTick();
        }

        // ---------------- Read PA15 button ----------------
        if (!(GPIOA->IDR & BUTTON_PA15)) {
            // Read last counter from EEPROM
            uint16_t last_counter = (EEPROM_ReadByte(eeprom_addr) << 8) |
                                     EEPROM_ReadByte(eeprom_addr + 1);

            char buffer[17];
            snprintf(buffer, sizeof(buffer), "Count: %u", last_counter);

            lprint(0x80, buffer);   // Line1
            lprint(0xC0, "Press again"); // Line2

            delay_ms(200); // debounce
        }
    }
}
