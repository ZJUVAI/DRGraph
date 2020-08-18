#!/usr/bin/env bash
cd build
export DRGRAPH_GPU_COMPILE=ON
cp ../src/data.cpp ../src/data.cu
cp ../src/visualizemod.cpp ../src/visualizemod.cu
cmake ..
make -j8
cd ..
