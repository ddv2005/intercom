#!/bin/bash
cd /projects/intercom/intercom
rm -R ./build
mkdir build
cd ./build
cmake ..
make -j5