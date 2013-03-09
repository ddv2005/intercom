export ARCH_POSTFIX="arm-unknown-linux-gnu"
export TOOLCHAIN_PREFIX=/opt/gcc-4.7/bin/arm-linux-gnueabi-
export CC=/opt/gcc-4.7/bin/arm-linux-gnueabi-gcc
export CXX=/opt/gcc-4.7/bin/arm-linux-gnueabi-g++
export AR="/opt/gcc-4.7/bin/arm-linux-gnueabi-ar"
export RANLIB=/opt/gcc-4.7/bin/arm-linux-gnueabi-ranlib
export AS=/opt/gcc-4.7/bin/arm-linux-gnueabi-as
#export LD=/opt/gcc-4.7/bin/arm-linux-gnueabi-ld
export STRIP=/opt/gcc-4.7/bin/arm-linux-gnueabi-strip

export CFLAGS="-O3 -fomit-frame-pointer -mfloat-abi=softfp -mfpu=neon -mcpu=cortex-a9 -ftree-vectorize -ffast-math -fmessage-length=0 -fdata-sections -ffunction-sections -marm"
export CXXFLAGS="-O3 -fomit-frame-pointer -mfloat-abi=softfp -mfpu=neon -mcpu=cortex-a9 -ftree-vectorize -ffast-math -fmessage-length=0 -fdata-sections -ffunction-sections -marm"
