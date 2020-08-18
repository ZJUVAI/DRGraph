#!/usr/bin/env bash
cd build
unset DRGRAPH_GPU_COMPILE
cmake ..
make -j8
cd ..
