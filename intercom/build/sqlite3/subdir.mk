################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../sqlite3/lsqlite3.c \
../sqlite3/sqlite3.c 

OBJS += \
./sqlite3/lsqlite3.o \
./sqlite3/sqlite3.o 

C_DEPS += \
./sqlite3/lsqlite3.d \
./sqlite3/sqlite3.d 


# Each subdirectory must supply rules for building sources it contributes
sqlite3/%.o: ../sqlite3/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross GCC Compiler'
	gcc -DPJ_IS_LITTLE_ENDIAN=1 -DPJ_IS_BIG_ENDIAN=0 -I"/projects/intercom/intercom/lua/src" -O3 -Wall -c -fmessage-length=0 -fvisibility=hidden -fdata-sections -ffunction-sections -fomit-frame-pointer -mfloat-abi=hard -mfpu=neon -mcpu=cortex-a9 -ftree-vectorize -marm -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


