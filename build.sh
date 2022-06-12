#!/usr/bin/env bash
cd build
cmake ..
make -j16
cd ..

/usr/bin/time -f "[MONITOR] MaxMemory: %M KB RealTime: %es UserTime: %Us KernelTime: %Ss" ./Vis -i ../../datasets/graph/Flan_1565.graph -o 2 --method GL
