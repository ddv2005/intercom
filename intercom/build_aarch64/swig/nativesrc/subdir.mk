################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../swig/nativesrc/intercom_lua_wrap.cpp 

OBJS += \
./swig/nativesrc/intercom_lua_wrap.o 

CPP_DEPS += \
./swig/nativesrc/intercom_lua_wrap.d 


# Each subdirectory must supply rules for building sources it contributes
swig/nativesrc/%.o: ../swig/nativesrc/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: Cross G++ Compiler'
	g++ -DPJ_AUTOCONF=1 -DPJ_IS_LITTLE_ENDIAN=1 -DPJ_IS_BIG_ENDIAN=0 -I"/projects/intercom/intercom/lua/src" -I/projects/intercom/3rdparty/pjproject/pjlib/include -I"/projects/intercom/intercom/pjsupport/include" -I/projects/intercom/3rdparty/pjproject/pjlib-util/include -I/projects/intercom/3rdparty/pjproject/pjmedia/include -I/projects/intercom/3rdparty/pjproject/pjnath/include -I/projects/intercom/3rdparty/pjproject/pjsip/include -I/projects/intercom/3rdparty/pjproject/third_party/speex/include -I/projects/intercom/3rdparty/pjproject/third_party/build/speex -O3 -Wall -c -fno-exceptions -fmessage-length=0 -fvisibility=hidden -fvisibility-inlines-hidden -fdata-sections -ffunction-sections -fomit-frame-pointer -ftree-vectorize -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


