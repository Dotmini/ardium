#!/bin/bash
set -e

INPUT_FILE=${1:-apps/hybrid_app.ar}
APP_NAME=$(basename "$INPUT_FILE" .ar)

echo "🔹 [1/4] Compiling Runtime (Objective-C++20)..."
clang++ -c runtime/src/runtime_v3.mm -std=c++20 -O3 -o runtime_v3.o

echo "🔹 [2/4] Compiling Ardium App ($INPUT_FILE)..."
_build/default/bin/main.exe build "$INPUT_FILE" -o "$APP_NAME" -c

# Rename .o (bitcode) to .bc so clang knows to compile it
mv "$APP_NAME.o" "$APP_NAME.bc"

echo "🔹 [3/4] Linking ($APP_NAME.bc + Runtime Object)..."
clang++ "$APP_NAME.bc" runtime_v3.o -framework Cocoa -framework Foundation -O3 -o "$APP_NAME"

echo "🔹 [4/4] Launching..."
echo "------------------------------------------------"
./"$APP_NAME"
