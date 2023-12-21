################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../echo_canceller.cpp \
../intercom.cpp \
../pjs_arduino_controller.cpp \
../pjs_audio_combine.cpp \
../pjs_audio_monitor.cpp \
../pjs_audio_monitor_zeromq.cpp \
../pjs_emulator_controller.cpp \
../pjs_external_controller.cpp \
../pjs_frame_processor.cpp \
../pjs_intercom.cpp \
../pjs_intercom_script_interface.cpp \
../pjs_lua.cpp \
../pjs_lua_libs.cpp \
../pjs_port.cpp \
../pjs_vr.cpp \
../project_config.cpp 

OBJS += \
./echo_canceller.o \
./intercom.o \
./pjs_arduino_controller.o \
./pjs_audio_combine.o \
./pjs_audio_monitor.o \
./pjs_audio_monitor_zeromq.o \
./pjs_emulator_controller.o \
./pjs_external_controller.o \
./pjs_frame_processor.o \
./pjs_intercom.o \
./pjs_intercom_script_interface.o \
./pjs_lua.o \
./pjs_lua_libs.o \
./pjs_port.o \
./pjs_vr.o \
./project_config.o 

CPP_DEPS += \
./echo_canceller.d \
./intercom.d \
./pjs_arduino_controller.d \
./pjs_audio_combine.d \
./pjs_audio_monitor.d \
./pjs_audio_monitor_zeromq.d \
./pjs_emulator_controller.d \
./pjs_external_controller.d \
./pjs_frame_processor.d \
./pjs_intercom.d \
./pjs_intercom_script_interface.d \
./pjs_lua.d \
./pjs_lua_libs.d \
./pjs_port.d \
./pjs_vr.d \
./project_config.d 


# Each subdirectory must supply rules for building sources it contributes
%.o: ../%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: Cross G++ Compiler'
	g++ -DPJ_AUTOCONF=1 -DPJ_IS_LITTLE_ENDIAN=1 -DPJ_IS_BIG_ENDIAN=0 -I"/projects/intercom/intercom/lua/src" -I/projects/intercom/3rdparty/pjproject/pjlib/include -I"/projects/intercom/intercom/pjsupport/include" -I/projects/intercom/3rdparty/pjproject/pjlib-util/include -I/projects/intercom/3rdparty/pjproject/pjmedia/include -I/projects/intercom/3rdparty/pjproject/pjnath/include -I/projects/intercom/3rdparty/pjproject/pjsip/include -I/projects/intercom/3rdparty/pjproject/third_party/speex/include -I/projects/intercom/3rdparty/pjproject/third_party/build/speex -O3 -Wall -c -fno-exceptions -fmessage-length=0 -fvisibility=hidden -fvisibility-inlines-hidden -fdata-sections -ffunction-sections -fomit-frame-pointer -ftree-vectorize -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


