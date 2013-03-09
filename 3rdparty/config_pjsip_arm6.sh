. ../build/set_arm6.sh
export AR="/opt/gcc-rhf/bin/arm-unknown-linux-gnueabi-ar rv"
cd pjproject
./configure --host=arm-pc-linux --disable-ipp --disable-ssl --disable-oss --disable-l16-codec --disable-gsm-codec --disable-g722-codec --disable-g7221-codec --disable-ilbc-codec --disable-opencore-amrnb --disable-speex-codec