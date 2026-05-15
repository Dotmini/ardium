#!/bin/bash
set -e

echo "🔹 [1/5] Compiling Runtime v4.0 (Objective-C++20)..."
clang++ -c runtime/src/runtime_v4.mm -std=c++20 -O3 -fobjc-arc -o runtime_v4.o

echo "🔹 [2/5] Compiling Metal Shaders (Skipping due to CI toolchain limits, using embedded fallback)..."
# /Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/metal -c shaders.metal -o shaders.air
# /Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/metallib shaders.air -o default.metallib

echo "🔹 [3/5] Compiling Ardium Logic (Dynamic Library)..."
# Compile to bitcode first (-c)
_build/default/bin/main.exe build apps/logic.ar -o logic -c
mv logic.o logic.bc
# Link bitcode to dylib
clang -dynamiclib -undefined dynamic_lookup logic.bc -o logic.dylib

echo "🔹 [4/5] Compiling Main App..."
_build/default/bin/main.exe build apps/main_v4.ar -o main_v4 -c
mv main_v4.o main_v4.bc

echo "🔹 [5/5] Linking App..."
clang++ main_v4.bc runtime_v4.o -framework Cocoa -framework Metal -framework MetalKit -framework Vision -framework AVFoundation -O3 -o App_v4

echo "🔹 [RUN] Launching Ardium v4.0..."
echo "------------------------------------------------"
./App_v4
