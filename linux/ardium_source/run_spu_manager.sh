#!/bin/bash
set -e

APP_NAME="SPU_Manager"

echo "🔹 [1/4] Compiling Runtime v5.0 (Objective-C++20)..."
# Re-compile runtime to ensure fresh state or use existing libTitan.a
# For speed in this test script, we assume libTitan.a exists from previous steps,
# but to be safe we link the objects or the lib.
# Let's link against the previously built libTitan.a

echo "🔹 [2/4] Compiling SPU Manager Script..."
_build/default/bin/main.exe build apps/SPU_Manager.ar -o spu_manager -c

# Rename to .bc
mv spu_manager.o spu_manager.bc

echo "🔹 [3/4] Linking..."
clang++ spu_manager.bc -L. -lTitan -framework Cocoa -framework Metal -framework MetalKit -framework Vision -framework CoreML -framework CoreVideo -framework Foundation -O3 -o "$APP_NAME"

echo "🔹 [4/4] Launching..."
echo "------------------------------------------------"
./"$APP_NAME"
