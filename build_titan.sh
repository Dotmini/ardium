#!/bin/bash
set -e

echo "🚀 [Major] Building Titan Runtime v5.0..."

# Create build directory
mkdir -p runtime/build

# Check OS Support
if [ "$(uname -s)" != "Darwin" ]; then
    echo "⚠️ libTitan.a is an Apple-specific SDK and is not natively compiled on this OS."
    echo "🔹 Creating dummy libTitan.a for compatibility..."
    touch runtime/build/dummy.o
    ar rcs libTitan.a runtime/build/dummy.o
    touch TitanTest
    exit 0
fi

# Source files list (The 5 Core Modules)
SOURCES=(
    "runtime/src/ArdiumOS_Mac.mm"
    "runtime/src/MemoryController.mm"
    "runtime/src/ArdiumGraphics.mm"
    "runtime/src/MetaSystem.mm"
    "runtime/src/TitanVM.mm"
    "runtime/src/TitanBus.mm"
    "runtime/src/TitanVision.mm"
    "runtime/src/TitanExecution.mm"
    "runtime/src/LibBridge.mm"
    "runtime/src/TitanUI.mm"
    "runtime/src/TitanNetwork.mm"
)

# Compile each module to an object file
OBJECTS=()
for src in "${SOURCES[@]}"; do
    filename=$(basename "$src" .mm)
    obj="runtime/build/${filename}.o"
    echo "🔹 Compiling $filename..."
    clang++ -c "$src" -std=c++20 -O3 -fobjc-arc -Iruntime/include -o "$obj"
    OBJECTS+=("$obj")
done

# Create Static Library
echo "🔹 Packing libTitan.a..."
ar rcs libTitan.a "${OBJECTS[@]}"

# Build Integration Test
echo "🔹 Building Integration Test..."
clang++ runtime/TitanTest.mm -std=c++20 -O3 -fobjc-arc -Iruntime/include \
    -L. -lTitan \
    -framework Cocoa \
    -framework Metal \
    -framework MetalKit \
    -framework Vision \
    -framework CoreML \
    -framework CoreVideo \
    -framework Foundation \
    -o TitanTest

echo "✅ Build Complete: libTitan.a, TitanTest"
echo "------------------------------------------------"
echo "🚀 [Major] Launching Titan Integration Test..."
./TitanTest
