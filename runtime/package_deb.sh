#!/bin/bash
set -e

# Configuration
VERSION="2.0.0"
ARCH="amd64" # Default to amd64 for Linux, can be overwritten (e.g. arm64)
PACKAGE_NAME="ardium-runtime"
FULL_NAME="${PACKAGE_NAME}_${VERSION}_${ARCH}"
BUILD_DIR="runtime/build"
DEB_DIR="$BUILD_DIR/$FULL_NAME"

# OS Check
OS=$(uname -s)
if [ "$OS" != "Linux" ]; then
    echo "⚠️  Warning: You are running this on $OS."
    echo "   Debian package creation requires 'dpkg-deb' which is typically on Linux."
    echo "   This script might fail if tools are missing."
fi

# Ensure Runtime is Built
echo "🔨 Ensuring Runtime is built..."
./runtime/build_runtime.sh

if [ ! -f "$BUILD_DIR/libardium.so" ]; then
    echo "❌ Error: runtime/build/libardium.so not found."
    echo "   Make sure you are building on Linux."
    exit 1
fi

echo "📦 Creating Debian Package Structure..."
rm -rf "$DEB_DIR"
mkdir -p "$DEB_DIR/usr/local/lib"
mkdir -p "$DEB_DIR/usr/local/include"
mkdir -p "$DEB_DIR/DEBIAN"

# Copy Files
cp "$BUILD_DIR/libardium.so" "$DEB_DIR/usr/local/lib/"
# Optional: Copy headers if we had them
# cp runtime/src/*.h "$DEB_DIR/usr/local/include/" 2>/dev/null || true

# Create Control File
echo "📝 Generating Control File..."
cat > "$DEB_DIR/DEBIAN/control" <<EOF
Package: $PACKAGE_NAME
Version: $VERSION
Section: libs
Priority: optional
Architecture: $ARCH
Maintainer: Ardium Team <team@ardium-lang.org>
Description: Ardium Language Runtime (v$VERSION)
 The shared runtime library for the Ardium programming language.
 Includes support for IO, Memory Management, and Concurrency.
EOF

# Build Package
echo "🔨 Building .deb package..."
dpkg-deb --build "$DEB_DIR"

echo "✅ Package Created: $BUILD_DIR/${FULL_NAME}.deb"
echo "   Install with: sudo dpkg -i $BUILD_DIR/${FULL_NAME}.deb"
