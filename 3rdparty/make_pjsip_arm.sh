cd pjproject
export CC=/opt/gcc-4.7/bin/arm-linux-gnueabi-gcc
export CXX=/opt/gcc-4.7/bin/arm-linux-gnueabi-g++
export AR="/opt/gcc-4.7/bin/arm-linux-gnueabi-ar rv"
export RANLIB=/opt/gcc-4.7/bin/arm-linux-gnueabi-ranlib
export TOOL_PREFIX=/opt/gcc-4.7/bin/arm-linux-gnueabi-
#make clean
make CFLAGS="-O3 -fomit-frame-pointer -mfloat-abi=softfp -mfpu=neon -mcpu=cortex-a9 -ftree-vectorize -ffast-math -c -fmessage-length=0 -fdata-sections -ffunction-sections -marm" -j1
cd third_party/build/speex
#make clean
make CFLAGS="-O3 -fomit-frame-pointer -mfloat-abi=softfp -mfpu=neon -mcpu=cortex-a9 -ftree-vectorize -ffast-math -c -fmessage-length=0 -fdata-sections -ffunction-sections -marm" -j1
