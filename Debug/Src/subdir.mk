################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/ClientSocket.c \
../src/ModbusExportedAPI.c \
../src/SessionControl.c \
../src/gpio_service.c \
../src/osalLinux.c 

OBJS += \
./src/ClientSocket.o \
./src/ModbusExportedAPI.o \
./src/SessionControl.o \
./src/gpio_service.o \
./src/osalLinux.o 

C_DEPS += \
./src/ClientSocket.d \
./src/ModbusExportedAPI.d \
./src/SessionControl.d \
./src/gpio_service.d \
./src/osalLinux.d 


# Each subdirectory must supply rules for building sources it contributes
src/%.o: ../src/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross GCC Compiler'
	aarch64-tdx-linux-gcc  -fstack-protector-strong  -D_FORTIFY_SOURCE=2 -Wformat -Wformat-security -Werror=format-security --sysroot=/opt/tdx-xwayland/5.7.0/sysroots/aarch64-tdx-linux -I"/home/aditya/usbmsd/Modbusclient/sharedlib/inc" -O0 -g3 -Wall -O2 -pipe -g -feliminate-unused-debug-types  -c -fPIC -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


