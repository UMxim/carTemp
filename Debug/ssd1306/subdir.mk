################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (11.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../ssd1306/ssd1306.c \
../ssd1306/ssd1306_fonts.c 

OBJS += \
./ssd1306/ssd1306.o \
./ssd1306/ssd1306_fonts.o 

C_DEPS += \
./ssd1306/ssd1306.d \
./ssd1306/ssd1306_fonts.d 


# Each subdirectory must supply rules for building sources it contributes
ssd1306/%.o ssd1306/%.su ssd1306/%.cyclo: ../ssd1306/%.c ssd1306/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m0plus -std=gnu11 -g3 -DDEBUG -DSTM32L011xx -DUSE_FULL_LL_DRIVER -DHSE_VALUE=8000000 -DHSE_STARTUP_TIMEOUT=100 -DLSE_STARTUP_TIMEOUT=5000 -DLSE_VALUE=32768 -DMSI_VALUE=2097000 -DHSI_VALUE=16000000 -DLSI_VALUE=37000 -DVDD_VALUE=3300 -DPREFETCH_ENABLE=0 -DINSTRUCTION_CACHE_ENABLE=1 -DDATA_CACHE_ENABLE=1 -c -I../Core/Inc -I../Drivers/STM32L0xx_HAL_Driver/Inc -I../Drivers/CMSIS/Device/ST/STM32L0xx/Include -I../Drivers/CMSIS/Include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"

clean: clean-ssd1306

clean-ssd1306:
	-$(RM) ./ssd1306/ssd1306.cyclo ./ssd1306/ssd1306.d ./ssd1306/ssd1306.o ./ssd1306/ssd1306.su ./ssd1306/ssd1306_fonts.cyclo ./ssd1306/ssd1306_fonts.d ./ssd1306/ssd1306_fonts.o ./ssd1306/ssd1306_fonts.su

.PHONY: clean-ssd1306

