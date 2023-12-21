################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../lua/luasocket/auxiliar.c \
../lua/luasocket/buffer.c \
../lua/luasocket/compat_lua.c \
../lua/luasocket/except.c \
../lua/luasocket/inet.c \
../lua/luasocket/io.c \
../lua/luasocket/luasocket.c \
../lua/luasocket/mime.c \
../lua/luasocket/options.c \
../lua/luasocket/select.c \
../lua/luasocket/tcp.c \
../lua/luasocket/timeout.c \
../lua/luasocket/udp.c \
../lua/luasocket/unix.c \
../lua/luasocket/usocket.c 

OBJS += \
./lua/luasocket/auxiliar.o \
./lua/luasocket/buffer.o \
./lua/luasocket/compat_lua.o \
./lua/luasocket/except.o \
./lua/luasocket/inet.o \
./lua/luasocket/io.o \
./lua/luasocket/luasocket.o \
./lua/luasocket/mime.o \
./lua/luasocket/options.o \
./lua/luasocket/select.o \
./lua/luasocket/tcp.o \
./lua/luasocket/timeout.o \
./lua/luasocket/udp.o \
./lua/luasocket/unix.o \
./lua/luasocket/usocket.o 

C_DEPS += \
./lua/luasocket/auxiliar.d \
./lua/luasocket/buffer.d \
./lua/luasocket/compat_lua.d \
./lua/luasocket/except.d \
./lua/luasocket/inet.d \
./lua/luasocket/io.d \
./lua/luasocket/luasocket.d \
./lua/luasocket/mime.d \
./lua/luasocket/options.d \
./lua/luasocket/select.d \
./lua/luasocket/tcp.d \
./lua/luasocket/timeout.d \
./lua/luasocket/udp.d \
./lua/luasocket/unix.d \
./lua/luasocket/usocket.d 


# Each subdirectory must supply rules for building sources it contributes
lua/luasocket/%.o: ../lua/luasocket/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross GCC Compiler'
	gcc -DPJ_IS_LITTLE_ENDIAN=1 -DPJ_IS_BIG_ENDIAN=0 -I"/projects/intercom/intercom/lua/src" -O3 -Wall -c -fmessage-length=0 -fvisibility=hidden -fdata-sections -ffunction-sections -fomit-frame-pointer -ftree-vectorize -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


