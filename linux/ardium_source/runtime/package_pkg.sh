#!/bin/bash
set -e

# Config
PKG_ROOT="runtime/pkg_root"
BUILD_DIR="runtime/build"
OUTPUT_PKG="runtime/build/ArdiumRuntime_2.0.0.pkg"
LIB_NAME="libardium.dylib"
IDENTIFIER="com.ardium.runtime"
VERSION="2.0.0"

echo "📦 Preparing Package Staging Area..."

# Clean and create
rm -rf "$PKG_ROOT"
mkdir -p "$PKG_ROOT/usr/local/lib"
mkdir -p "$PKG_ROOT/usr/local/lib/ardium/stdlib"

# Copy Runtime Library
cp "$BUILD_DIR/$LIB_NAME" "$PKG_ROOT/usr/local/lib/"

# Copy Standard Library
# Assuming stdlib is in the root directory
if [ -d "stdlib" ]; then
    cp -R stdlib/* "$PKG_ROOT/usr/local/lib/ardium/stdlib/"
else
    echo "⚠️ Warning: stdlib directory not found!"
fi

echo "📦 Building Component Package..."

pkgbuild --root "$PKG_ROOT" \
         --identifier "$IDENTIFIER" \
         --version "$VERSION" \
         --install-location "/" \
         "$OUTPUT_PKG"

echo "✅ Created Installer: $OUTPUT_PKG"
ls -lh "$OUTPUT_PKG"
