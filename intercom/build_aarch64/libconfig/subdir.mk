################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../libconfig/libconfigcpp.cpp 

C_SRCS += \
../libconfig/grammar.c \
../libconfig/libconfig.c \
../libconfig/scanctx.c \
../libconfig/scanner.c \
../libconfig/strbuf.c 

OBJS += \
./libconfig/grammar.o \
./libconfig/libconfig.o \
./libconfig/libconfigcpp.o \
./libconfig/scanctx.o \
./libconfig/scanner.o \
./libconfig/strbuf.o 

C_DEPS += \
./libconfig/grammar.d \
./libconfig/libconfig.d \
./libconfig/scanctx.d \
./libconfig/scanner.d \
./libconfig/strbuf.d 

CPP_DEPS += \
./libconfig/libconfigcpp.d 


# Each subdirectory must supply rules for building sources it contributes
libconfig/%.o: ../libconfig/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross GCC Compiler'
	gcc -DPJ_AUTOCONF=1 -DPJ_IS_LITTLE_ENDIAN=1 -DPJ_IS_BIG_ENDIAN=0 -I"/projects/intercom/intercom/lua/src" -O3 -Wall -c -fmessage-length=0 -fvisibility=hidden -fdata-sections -ffunction-sections -fomit-frame-pointer -ftree-vectorize  -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

libconfig/%.o: ../libconfig/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: Cross G++ Compiler'
	g++ -DPJ_AUTOCONF=1 -DPJ_IS_LITTLE_ENDIAN=1 -DPJ_IS_BIG_ENDIAN=0 -I"/projects/intercom/intercom/lua/src" -I/projects/intercom/3rdparty/pjproject/pjlib/include -I"/projects/intercom/intercom/pjsupport/include" -I/projects/intercom/3rdparty/pjproject/pjlib-util/include -I/projects/intercom/3rdparty/pjproject/pjmedia/include -I/projects/intercom/3rdparty/pjproject/pjnath/include -I/projects/intercom/3rdparty/pjproject/pjsip/include -I/projects/intercom/3rdparty/pjproject/third_party/speex/include -I/projects/intercom/3rdparty/pjproject/third_party/build/speex -O3 -Wall -c -fno-exceptions -fmessage-length=0 -fvisibility=hidden -fvisibility-inlines-hidden -fdata-sections -ffunction-sections -fomit-frame-pointer -ftree-vectorize  -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


