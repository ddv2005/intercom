export ARCH_POSTFIX="arm-pc-linux-gnu"
export TOOLCHAIN_PREFIX=/opt/gcc-rhf/bin/arm-unknown-linux-gnueabi-
export CC=/opt/gcc-rhf/bin/arm-unknown-linux-gnueabi-gcc
export CXX=/opt/gcc-rhf/bin/arm-unknown-linux-gnueabi-g++
export AR="/opt/gcc-rhf/bin/arm-unknown-linux-gnueabi-ar"
export RANLIB=/opt/gcc-rhf/bin/arm-unknown-linux-gnueabi-ranlib
export AS=/opt/gcc-rhf/bin/arm-unknown-linux-gnueabi-as
#export LD=/opt/gcc-rhf/bin/arm-unknown-linux-gnueabi-ld
export STRIP=/opt/gcc-rhf/bin/arm-unknown-linux-gnueabi-strip

export CFLAGS="-O3 -fomit-frame-pointer -mfloat-abi=hard -march=armv6j -mtune=arm1176jzf-s -mfpu=vfp -ftree-vectorize -ffast-math -fmessage-length=0 -fdata-sections -ffunction-sections -marm"
export CXXFLAGS="-O3 -fomit-frame-pointer -mfloat-abi=hard -march=armv6j -mtune=arm1176jzf-s -mfpu=vfp -ftree-vectorize -ffast-math -fmessage-length=0 -fdata-sections -ffunction-sections -marm"
export LDFLAGS="-Wl,-dynamic-linker=/lib/ld-linux-armhf.so.3"