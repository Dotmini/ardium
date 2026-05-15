#!/bin/bash
set -e

echo "Building Graphics Backend..."

# Ensure the output directory exists
mkdir -p lib

# Compile CoreUI backend (C++)
clang++ -c runtime/src/CoreUI.cpp \
    -o lib/CoreUI.o \
    -std=c++17 \
    -I./metal-cpp \
    -DNS_PRIVATE_IMPLEMENTATION \
    -DCA_PRIVATE_IMPLEMENTATION \
    -DMTL_PRIVATE_IMPLEMENTATION \
    -framework Metal \
    -framework Foundation \
    -framework QuartzCore

# Compile CoreUI Mac Shim (Obj-C++)
clang++ -c runtime/src/CoreUI_Mac.mm \
    -o lib/CoreUI_Mac.o \
    -std=c++17 \
    -fobjc-arc \
    -framework Cocoa \
    -framework QuartzCore

# Create static library
ar rcs lib/libCoreUI.a lib/CoreUI.o lib/CoreUI_Mac.o

echo "CoreUI Library Built: lib/libCoreUI.a"
