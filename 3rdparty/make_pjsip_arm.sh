. ../build/set_arm_hf.sh
export AR="${AR} rv"

cd pjproject
make clean
make -j1
cd third_party/build/speex
make clean
make -j1
