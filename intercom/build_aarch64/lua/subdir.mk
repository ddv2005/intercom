################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../lua/lmarshal.c 

OBJS += \
./lua/lmarshal.o 

C_DEPS += \
./lua/lmarshal.d 


# Each subdirectory must supply rules for building sources it contributes
lua/%.o: ../lua/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross GCC Compiler'
	gcc -DPJ_IS_LITTLE_ENDIAN=1 -DPJ_IS_BIG_ENDIAN=0 -I"/projects/intercom/intercom/lua/src" -O3 -Wall -c -fmessage-length=0 -fvisibility=hidden -fdata-sections -ffunction-sections -fomit-frame-pointer -ftree-vectorize -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


