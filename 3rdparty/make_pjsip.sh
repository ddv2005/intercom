cd pjproject
make clean
make CFLAGS="-fomit-frame-pointer -ftree-vectorize -ffast-math -fmessage-length=0 -fdata-sections -ffunction-sections" -j1
cd third_party/build/speex
make clean
make CFLAGS="-fomit-frame-pointer -ftree-vectorize -ffast-math -fmessage-length=0 -fdata-sections -ffunction-sections" -j1