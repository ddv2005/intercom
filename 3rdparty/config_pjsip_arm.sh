. ../build/set_arm_hf.sh
export AR="${AR} rv"
cd pjproject
./configure --host=arm-linux --disable-ipp --disable-ssl --disable-oss --disable-l16-codec --disable-gsm-codec --disable-g722-codec --disable-g7221-codec --disable-ilbc-codec --disable-opencore-amrnb --disable-speex-codec