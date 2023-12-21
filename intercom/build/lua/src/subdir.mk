################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../lua/src/lapi.c \
../lua/src/lauxlib.c \
../lua/src/lbaselib.c \
../lua/src/lbitlib.c \
../lua/src/lcode.c \
../lua/src/lcorolib.c \
../lua/src/lctype.c \
../lua/src/ldblib.c \
../lua/src/ldebug.c \
../lua/src/ldo.c \
../lua/src/ldump.c \
../lua/src/lfunc.c \
../lua/src/lgc.c \
../lua/src/linit.c \
../lua/src/liolib.c \
../lua/src/llex.c \
../lua/src/lmathlib.c \
../lua/src/lmem.c \
../lua/src/loadlib.c \
../lua/src/lobject.c \
../lua/src/lopcodes.c \
../lua/src/loslib.c \
../lua/src/lparser.c \
../lua/src/lstate.c \
../lua/src/lstring.c \
../lua/src/lstrlib.c \
../lua/src/ltable.c \
../lua/src/ltablib.c \
../lua/src/ltm.c \
../lua/src/lundump.c \
../lua/src/lvm.c \
../lua/src/lzio.c 

OBJS += \
./lua/src/lapi.o \
./lua/src/lauxlib.o \
./lua/src/lbaselib.o \
./lua/src/lbitlib.o \
./lua/src/lcode.o \
./lua/src/lcorolib.o \
./lua/src/lctype.o \
./lua/src/ldblib.o \
./lua/src/ldebug.o \
./lua/src/ldo.o \
./lua/src/ldump.o \
./lua/src/lfunc.o \
./lua/src/lgc.o \
./lua/src/linit.o \
./lua/src/liolib.o \
./lua/src/llex.o \
./lua/src/lmathlib.o \
./lua/src/lmem.o \
./lua/src/loadlib.o \
./lua/src/lobject.o \
./lua/src/lopcodes.o \
./lua/src/loslib.o \
./lua/src/lparser.o \
./lua/src/lstate.o \
./lua/src/lstring.o \
./lua/src/lstrlib.o \
./lua/src/ltable.o \
./lua/src/ltablib.o \
./lua/src/ltm.o \
./lua/src/lundump.o \
./lua/src/lvm.o \
./lua/src/lzio.o 

C_DEPS += \
./lua/src/lapi.d \
./lua/src/lauxlib.d \
./lua/src/lbaselib.d \
./lua/src/lbitlib.d \
./lua/src/lcode.d \
./lua/src/lcorolib.d \
./lua/src/lctype.d \
./lua/src/ldblib.d \
./lua/src/ldebug.d \
./lua/src/ldo.d \
./lua/src/ldump.d \
./lua/src/lfunc.d \
./lua/src/lgc.d \
./lua/src/linit.d \
./lua/src/liolib.d \
./lua/src/llex.d \
./lua/src/lmathlib.d \
./lua/src/lmem.d \
./lua/src/loadlib.d \
./lua/src/lobject.d \
./lua/src/lopcodes.d \
./lua/src/loslib.d \
./lua/src/lparser.d \
./lua/src/lstate.d \
./lua/src/lstring.d \
./lua/src/lstrlib.d \
./lua/src/ltable.d \
./lua/src/ltablib.d \
./lua/src/ltm.d \
./lua/src/lundump.d \
./lua/src/lvm.d \
./lua/src/lzio.d 


# Each subdirectory must supply rules for building sources it contributes
lua/src/%.o: ../lua/src/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross GCC Compiler'
	gcc -DPJ_IS_LITTLE_ENDIAN=1 -DPJ_IS_BIG_ENDIAN=0 -I"/projects/intercom/intercom/lua/src" -O3 -Wall -c -fmessage-length=0 -fvisibility=hidden -fdata-sections -ffunction-sections -fomit-frame-pointer -mfloat-abi=hard -mfpu=neon -mcpu=cortex-a9 -ftree-vectorize -marm -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


