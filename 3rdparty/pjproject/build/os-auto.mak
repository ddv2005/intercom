# build/os-auto.mak.  Generated from os-auto.mak.in by configure.

export OS_CFLAGS   := $(CC_DEF)PJ_AUTOCONF=1 -O3 -fomit-frame-pointer -mfloat-abi=hard -march=armv6j -mtune=arm1176jzf-s -mfpu=vfp -ftree-vectorize -ffast-math -fmessage-length=0 -fdata-sections -ffunction-sections -marm -DPJ_IS_BIG_ENDIAN=0 -DPJ_IS_LITTLE_ENDIAN=1

export OS_CXXFLAGS := $(CC_DEF)PJ_AUTOCONF=1 -O3 -fomit-frame-pointer -mfloat-abi=hard -march=armv6j -mtune=arm1176jzf-s -mfpu=vfp -ftree-vectorize -ffast-math -fmessage-length=0 -fdata-sections -ffunction-sections -marm -O3 -fomit-frame-pointer -mfloat-abi=hard -march=armv6j -mtune=arm1176jzf-s -mfpu=vfp -ftree-vectorize -ffast-math -fmessage-length=0 -fdata-sections -ffunction-sections -marm

export OS_LDFLAGS  := -Wl,-dynamic-linker=/lib/ld-linux-armhf.so.3 -lm -lnsl -lrt -lpthread  -lasound

export OS_SOURCES  := 


