#!/bin/bash
set -e

# Configuration
RUNTIME_SRC="runtime/src"
BUILD_DIR="runtime/build"
OUTPUT_LIB="$BUILD_DIR/libardium.dylib"

# Ensure build directory exists
mkdir -p "$BUILD_DIR"

echo "🔨 Building Ardium Runtime Library..."

# Compile runtime.m (Objective-C/C) and runtime_core.cpp (C++) into a single dynamic library
# OS Detection
OS=$(uname -s)

if [ "$OS" == "Darwin" ]; then
    echo "🍎 Detected macOS. Building libardium.dylib..."
    
    clang++ -c -o "$BUILD_DIR/runtime_core.o" "$RUNTIME_SRC/runtime_core.cpp" -std=c++17 -fPIC
    clang++ -c -o "$BUILD_DIR/runtime_common.o" "$RUNTIME_SRC/runtime_common.cpp" -std=c++17 -fPIC
    clang -c -o "$BUILD_DIR/runtime.o" "$RUNTIME_SRC/runtime.m" -fobjc-arc -fPIC -DARDIUM_GUI_BUILD
    
    # Compile Swift Bridge
    swiftc -c "$RUNTIME_SRC/SwiftUIBridge.swift" -o "$BUILD_DIR/SwiftUIBridge.o" -parse-as-library -emit-dependencies -module-name ArdiumSwift

    if [ "$LITE_RUNTIME" == "1" ]; then
        echo "⚡️ Building LITE Ardium Runtime (No GUI)..."
        clang++ -dynamiclib -o "$OUTPUT_LIB" \
            "$BUILD_DIR/runtime_core.o" \
            "$BUILD_DIR/runtime_common.o" \
            -L/usr/lib/swift \
            -install_name "@rpath/libardium.dylib"
    else
        clang++ -dynamiclib -o "$OUTPUT_LIB" \
            "$BUILD_DIR/runtime_core.o" \
            "$BUILD_DIR/runtime.o" \
            "$BUILD_DIR/runtime_common.o" \
            "$BUILD_DIR/SwiftUIBridge.o" \
            -framework Cocoa -framework Foundation -framework SwiftUI -framework Metal \
            -L/usr/lib/swift \
            -install_name "@rpath/libardium.dylib"
    fi
        
    OUTPUT_NAME="libardium.dylib"

elif [ "$OS" == "Linux" ]; then
    echo "🐧 Detected Linux. Building libardium.so..."
    
    OUTPUT_LIB="$BUILD_DIR/libardium.so"
    
    clang++ -c -o "$BUILD_DIR/runtime_core.o" "$RUNTIME_SRC/runtime_core.cpp" -std=c++17 -fPIC
    clang++ -c -o "$BUILD_DIR/runtime_common.o" "$RUNTIME_SRC/runtime_common.cpp" -std=c++17 -fPIC
    
    # On Linux we don't compile runtime.m
    # We link with -shared, -lpthread, -ldl
    
    clang++ -shared -o "$OUTPUT_LIB" \
        "$BUILD_DIR/runtime_core.o" \
        "$BUILD_DIR/runtime_common.o" \
        -lpthread -ldl -lm
        
    OUTPUT_NAME="libardium.so"
else
    echo "❌ Underlying OS $OS not supported yet."
    exit 1
fi

echo "✅ Created $OUTPUT_LIB"

# Verify
ls -lh "$OUTPUT_LIB"
file "$OUTPUT_LIB"
