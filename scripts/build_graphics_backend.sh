#!/bin/bash
set -e

echo "Building Graphics Backend (Raylib)..."

mkdir -p lib

# 1. Fetch and build Raylib if not present
if [ ! -f "lib/libraylib.a" ]; then
    echo "Downloading Raylib 5.0..."
    rm -rf lib/raylib_src
    git clone --depth 1 --branch 5.0 https://github.com/raysan5/raylib.git lib/raylib_src
    
    echo "Compiling Raylib..."
    cd lib/raylib_src
    mkdir build && cd build
    cmake -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DBUILD_SHARED_LIBS=OFF -DBUILD_EXAMPLES=OFF -DCMAKE_BUILD_TYPE=Release ..
    make -j4
    
    cd ../../../
    cp lib/raylib_src/build/raylib/libraylib.a lib/
    cp -r lib/raylib_src/src lib/raylib_include
    echo "Raylib built successfully!"
fi

# 2. Compile CoreUI_Raylib.cpp
echo "Compiling CoreUI_Raylib..."
clang++ -c runtime/src/CoreUI_Raylib.cpp \
    -o lib/CoreUI.o \
    -std=c++17 \
    -Ilib/raylib_include

# 3. Create static library for CoreUI
ar rcs lib/libCoreUI.a lib/CoreUI.o

echo "CoreUI Library Built: lib/libCoreUI.a"
