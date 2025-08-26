#include "dht11.h"

GPIO_InitTypeDef GPIO_InitStruct = {0};

void DWT_Init(void) {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

void delay_us_tick(uint32_t us) {
    uint32_t startTick = DWT->CYCCNT;
    uint32_t delayTicks = us * (HAL_RCC_GetHCLKFreq() / 1000000);
    while ((DWT->CYCCNT - startTick) < delayTicks);
}

static void DHT11_Set_Pin_Output(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin) {
    GPIO_InitStruct.Pin = GPIO_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOx, &GPIO_InitStruct);
}

static void DHT11_Set_Pin_Input(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin) {
    GPIO_InitStruct.Pin = GPIO_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOx, &GPIO_InitStruct);
}

static void DHT11_Start(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin) {
    DHT11_Set_Pin_Output(GPIOx, GPIO_Pin);
    HAL_GPIO_WritePin(GPIOx, GPIO_Pin, GPIO_PIN_RESET);
    HAL_Delay(20);
    HAL_GPIO_WritePin(GPIOx, GPIO_Pin, GPIO_PIN_SET);
    delay_us_tick(30);
    DHT11_Set_Pin_Input(GPIOx, GPIO_Pin);
}

static uint8_t DHT11_Check_Response(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin) {
    uint8_t Response = 0;
    delay_us_tick(40);
    if (!(HAL_GPIO_ReadPin(GPIOx, GPIO_Pin))) {
        delay_us_tick(80);
        if ((HAL_GPIO_ReadPin(GPIOx, GPIO_Pin))) Response = 1;
        delay_us_tick(80);
    }
    return Response;
}

static uint8_t DHT11_Read(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin) {
    uint8_t i, data = 0;
    for (i = 0; i < 8; i++) {
        while (!(HAL_GPIO_ReadPin(GPIOx, GPIO_Pin)));
        delay_us_tick(40);
        if (HAL_GPIO_ReadPin(GPIOx, GPIO_Pin))
            data |= (1 << (7 - i));
        while (HAL_GPIO_ReadPin(GPIOx, GPIO_Pin));
    }
    return data;
}

uint8_t DHT11_GetData(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin, DHT11_Data *data) {
    uint8_t Rh_byte1, Rh_byte2, Temp_byte1, Temp_byte2, checksum;

    DHT11_Start(GPIOx, GPIO_Pin);
    if (DHT11_Check_Response(GPIOx, GPIO_Pin)) {
        Rh_byte1 = DHT11_Read(GPIOx, GPIO_Pin);
        Rh_byte2 = DHT11_Read(GPIOx, GPIO_Pin);
        Temp_byte1 = DHT11_Read(GPIOx, GPIO_Pin);
        Temp_byte2 = DHT11_Read(GPIOx, GPIO_Pin);
        checksum = DHT11_Read(GPIOx, GPIO_Pin);

        if (checksum == (Rh_byte1 + Rh_byte2 + Temp_byte1 + Temp_byte2)) {
            data->Humidity = Rh_byte1;
            data->Temperature = Temp_byte1;
            return 1;
        }
    }
    return 0;
}
