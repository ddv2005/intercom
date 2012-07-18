. ./set_arm.sh
make clean
make
mkdir arm
cp *.so arm/
cp mjpg_streamer arm/mjpg_streamer
