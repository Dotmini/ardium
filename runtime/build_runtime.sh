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
    # We compile the C++ stubs instead
    clang++ -c -o "$BUILD_DIR/runtime_gui_stubs.o" "$RUNTIME_SRC/runtime_gui_stubs.cpp" -std=c++17 -fPIC
    
    clang++ -shared -o "$OUTPUT_LIB" \
        "$BUILD_DIR/runtime_core.o" \
        "$BUILD_DIR/runtime_common.o" \
        "$BUILD_DIR/runtime_gui_stubs.o" \
        -lpthread -ldl -lm
        
    OUTPUT_NAME="libardium.so"
elif [[ "$OS" == *"MINGW"* ]] || [[ "$OS" == *"CYGWIN"* ]] || [[ "$OS" == *"MSYS"* ]]; then
    echo "🪟 Detected Windows. Building libardium.dll..."
    OUTPUT_LIB="$BUILD_DIR/libardium.dll"
    
    # Check if clang++ is available
    if command -v clang++ &> /dev/null; then
        clang++ -c -o "$BUILD_DIR/runtime_core.o" "$RUNTIME_SRC/runtime_core.cpp" -std=c++17
        clang++ -c -o "$BUILD_DIR/runtime_common.o" "$RUNTIME_SRC/runtime_common.cpp" -std=c++17
        clang++ -c -o "$BUILD_DIR/runtime_gui_stubs.o" "$RUNTIME_SRC/runtime_gui_stubs.cpp" -std=c++17
        clang++ -shared -o "$OUTPUT_LIB" "$BUILD_DIR/runtime_core.o" "$BUILD_DIR/runtime_common.o" "$BUILD_DIR/runtime_gui_stubs.o"
        OUTPUT_NAME="libardium.dll"
    else
        echo "⚠️ clang++ not found on Windows. Creating a dummy file so build can continue."
        touch "$OUTPUT_LIB"
    fi
else
    echo "❌ Underlying OS $OS not supported yet."
    exit 1
fi

echo "✅ Created $OUTPUT_LIB"

# Verify
ls -lh "$OUTPUT_LIB"
file "$OUTPUT_LIB"
