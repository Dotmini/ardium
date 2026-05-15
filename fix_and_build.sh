#!/bin/bash
set -e

echo "🔧 Fixing System Build Tools (ar)..."
# Check if ar is indeed a symlink to arc
if [ -L "/usr/local/bin/ar" ]; then
    echo "Requires sudo to remove invalid /usr/local/bin/ar symlink"
    sudo rm /usr/local/bin/ar
    echo "✅ /usr/local/bin/ar removed. System 'ar' restored."
else
    echo "✅ System 'ar' looks correct or not customized."
fi

echo "🚀 Building Ardium Compiler v2.3.2..."
dune build

echo "🔨 Building Runtime Library..."
./runtime/build_runtime.sh

echo "📦 Creating Release Bundle..."
VERSION="2.3.2"
DIST_DIR="dist/ardium-$VERSION"

# Clean
rm -rf "dist/ardium-$VERSION"
mkdir -p "$DIST_DIR/bin"
mkdir -p "$DIST_DIR/lib"
mkdir -p "$DIST_DIR/include"
mkdir -p "$DIST_DIR/stdlib"

# Copy Compiler
cp _build/default/bin/main.exe "$DIST_DIR/bin/arc"

# Copy Runtime
cp runtime/build/libardium.dylib "$DIST_DIR/lib/"

# Copy Standard Library
cp stdlib/*.ar "$DIST_DIR/stdlib/"

# Copy Headers (optional if we generate them)
# cp runtime/src/*.h "$DIST_DIR/include/"

echo "📜 Creating Archive..."
cd dist
tar -czf "ardium-$VERSION-macos-arm64.tar.gz" "ardium-$VERSION"
cd ..

echo "✅ Build Complete!"
echo "📍 Bundle Location: dist/ardium-$VERSION-macos-arm64.tar.gz"
echo "👉 To install globally: sudo cp $DIST_DIR/bin/arc /usr/local/bin/"
