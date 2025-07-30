#!/bin/bash
export SG200X_SDK_PATH=$HOME/recamera/sg2002_recamera_emmc/
export PATH=$HOME/recamera/host-tools/gcc/riscv64-linux-musl-x86_64/bin:$PATH
#test if build folder exists
if [ ! -d "build" ]; then
    mkdir build
fi

cd build
cmake ..
make
