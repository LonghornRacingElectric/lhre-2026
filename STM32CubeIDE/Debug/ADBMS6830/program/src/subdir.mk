################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (12.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../ADBMS6830/program/src/adBms_Application.c \
../ADBMS6830/program/src/mcuWrapper.c \
../ADBMS6830/program/src/serialPrintResult.c 

OBJS += \
./ADBMS6830/program/src/adBms_Application.o \
./ADBMS6830/program/src/mcuWrapper.o \
./ADBMS6830/program/src/serialPrintResult.o 

C_DEPS += \
./ADBMS6830/program/src/adBms_Application.d \
./ADBMS6830/program/src/mcuWrapper.d \
./ADBMS6830/program/src/serialPrintResult.d 


# Each subdirectory must supply rules for building sources it contributes
ADBMS6830/program/src/%.o ADBMS6830/program/src/%.su ADBMS6830/program/src/%.cyclo: ../ADBMS6830/program/src/%.c ADBMS6830/program/src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32G474xx -c -I"C:/Users/VGudluru/Documents/BroadMarket_Support_ADBMS6830/Core/Inc" -I../Drivers/STM32G4xx_HAL_Driver/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32G4xx/Include -I../Drivers/CMSIS/Include -I"C:/Users/VGudluru/Documents/BroadMarket_Support_ADBMS6830/ADBMS6830/program/inc" -I"C:/Users/VGudluru/Documents/BroadMarket_Support_ADBMS6830/ADBMS6830/lib/inc" -I../Core/Inc -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-ADBMS6830-2f-program-2f-src

clean-ADBMS6830-2f-program-2f-src:
	-$(RM) ./ADBMS6830/program/src/adBms_Application.cyclo ./ADBMS6830/program/src/adBms_Application.d ./ADBMS6830/program/src/adBms_Application.o ./ADBMS6830/program/src/adBms_Application.su ./ADBMS6830/program/src/mcuWrapper.cyclo ./ADBMS6830/program/src/mcuWrapper.d ./ADBMS6830/program/src/mcuWrapper.o ./ADBMS6830/program/src/mcuWrapper.su ./ADBMS6830/program/src/serialPrintResult.cyclo ./ADBMS6830/program/src/serialPrintResult.d ./ADBMS6830/program/src/serialPrintResult.o ./ADBMS6830/program/src/serialPrintResult.su

.PHONY: clean-ADBMS6830-2f-program-2f-src

