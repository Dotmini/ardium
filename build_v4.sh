#!/bin/bash
set -e

APP_NAME="GodView"

echo "🔹 [1/4] Compiling Runtime v4.0 (Objective-C++20)..."
clang++ -c runtime/src/runtime_v4.mm -std=c++20 -O3 -fobjc-arc -o runtime_v4.o

echo "🔹 [2/4] Compiling Ardium God View..."
_build/default/bin/main.exe build apps/god_view.ar -o god_view -c

# Rename .o to .bc
mv god_view.o god_view.bc

echo "🔹 [3/4] Linking..."
clang++ god_view.bc runtime_v4.o -framework Cocoa -framework Metal -framework MetalKit -framework Vision -framework AVFoundation -O3 -o "$APP_NAME"

echo "🔹 [4/4] Launching..."
echo "------------------------------------------------"
./"$APP_NAME"
