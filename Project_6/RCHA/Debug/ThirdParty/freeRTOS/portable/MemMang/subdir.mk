################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (10.3-2021.10)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../ThirdParty/freeRTOS/portable/MemMang/heap_4.c 

OBJS += \
./ThirdParty/freeRTOS/portable/MemMang/heap_4.o 

C_DEPS += \
./ThirdParty/freeRTOS/portable/MemMang/heap_4.d 


# Each subdirectory must supply rules for building sources it contributes
ThirdParty/freeRTOS/portable/MemMang/%.o ThirdParty/freeRTOS/portable/MemMang/%.su: ../ThirdParty/freeRTOS/portable/MemMang/%.c ThirdParty/freeRTOS/portable/MemMang/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F405xx -c -I../Core/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F4xx/Include -I../Drivers/CMSIS/Include -I"C:/Users/WZS1KOR/STM32CubeIDE/workspace_2/RCHA/ThirdParty/freeRTOS" -I"C:/Users/WZS1KOR/STM32CubeIDE/workspace_2/RCHA/ThirdParty/freeRTOS/include" -I"C:/Users/WZS1KOR/STM32CubeIDE/workspace_2/RCHA/ThirdParty/freeRTOS/portable/GCC/ARM_CM4F" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-ThirdParty-2f-freeRTOS-2f-portable-2f-MemMang

clean-ThirdParty-2f-freeRTOS-2f-portable-2f-MemMang:
	-$(RM) ./ThirdParty/freeRTOS/portable/MemMang/heap_4.d ./ThirdParty/freeRTOS/portable/MemMang/heap_4.o ./ThirdParty/freeRTOS/portable/MemMang/heap_4.su

.PHONY: clean-ThirdParty-2f-freeRTOS-2f-portable-2f-MemMang

