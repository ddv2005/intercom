. ../build/set_arm_hf.sh
make clean
make
mkdir arm
cp *.so arm/
cp mjpg_streamer arm/mjpg_streamer
