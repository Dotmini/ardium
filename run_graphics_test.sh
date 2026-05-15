#!/bin/bash
set -e

APP_NAME="TestGraphics"

echo "🔹 [1/4] Compiling Ardium Test App..."
_build/default/bin/main.exe build apps/test_graphics.ar -o test_graphics -c

# Rename to .bc
mv test_graphics.o test_graphics.bc

echo "🔹 [2/4] Linking with ArdiumGraphics..."
clang++ test_graphics.bc -L. -lArdiumGraphics -framework Cocoa -framework Metal -framework MetalKit -framework CoreVideo -O3 -o "$APP_NAME"

echo "🔹 [3/4] Launching..."
echo "------------------------------------------------"
./"$APP_NAME"
