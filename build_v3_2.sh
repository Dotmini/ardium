#!/bin/bash
set -e

APP_NAME="SimulationApp"

echo "🔹 [1/4] Compiling Runtime v3.2 (Objective-C++20)..."
clang++ -c runtime/src/runtime_v3_2.mm -std=c++20 -O3 -fobjc-arc -o runtime_v3_2.o

echo "🔹 [2/4] Compiling Ardium Simulation..."
_build/default/bin/main.exe build apps/simulation.ar -o simulation -c

# Rename .o (bitcode) to .bc
mv simulation.o simulation.bc

echo "🔹 [3/4] Linking..."
clang++ simulation.bc runtime_v3_2.o -framework Cocoa -framework Foundation -framework CoreGraphics -O3 -o "$APP_NAME"

echo "🔹 [4/4] Launching..."
echo "------------------------------------------------"
./"$APP_NAME"
