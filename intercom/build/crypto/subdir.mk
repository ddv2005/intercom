################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../crypto/mem_clr.c \
../crypto/sha256.c \
../crypto/sha512.c 

OBJS += \
./crypto/mem_clr.o \
./crypto/sha256.o \
./crypto/sha512.o 

C_DEPS += \
./crypto/mem_clr.d \
./crypto/sha256.d \
./crypto/sha512.d 


# Each subdirectory must supply rules for building sources it contributes
crypto/%.o: ../crypto/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross GCC Compiler'
	/opt/gcc-4.7.hf/bin/arm-linux-gnueabihf-gcc -DPJ_IS_LITTLE_ENDIAN=1 -DPJ_IS_BIG_ENDIAN=0 -I"/projects/intercom/intercom/lua/src" -O3 -Wall -c -fmessage-length=0 -fvisibility=hidden -fdata-sections -ffunction-sections -fomit-frame-pointer -mfloat-abi=hard -mfpu=neon -mcpu=cortex-a9 -ftree-vectorize -marm -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


