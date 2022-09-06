####################################################################################
# The source code contained or described herein and all documents related to
# the source code ("Material") are owned by Intel Corporation. Title to the
# Material remains with Intel Corporation.
#
# No license under any patent, copyright, trade secret or other intellectual
# property right is granted to or conferred upon you by disclosure or delivery of
# the Materials, either expressly, by implication, inducement, estoppel or otherwise.
####################################################################################


# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Src/ClientSocket.c \
../Src/ModbusExportedAPI.c \
../Src/SessionControl.c \
../Src/osalLinux.c 

OBJS += \
./Src/ClientSocket.o \
./Src/ModbusExportedAPI.o \
./Src/SessionControl.o \
./Src/osalLinux.o 

C_DEPS += \
./Src/ClientSocket.d \
./Src/ModbusExportedAPI.d \
./Src/SessionControl.d \
./Src/osalLinux.d 


# Each subdirectory must supply rules for building sources it contributes
Src/%.o: ../Src/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -std=c11 -DMODBUS_STACK_TCPIP_ENABLED -I..//Inc -I../../bin/safestring/include -O3 -Wall -c -fmessage-length=0 -fPIC -pthread  -O2 -D_FORTIFY_SOURCE=2 -static -fvisibility=hidden -Wformat -Wformat-security -fstack-protector-strong -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


