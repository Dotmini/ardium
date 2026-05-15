#!/bin/bash
set -e

# ==============================================================================
#  ARDIUM v2.5.5 DIST BUNDLER
# ==============================================================================
#  DevOps Engineer: Major
# ==============================================================================

DIST_DIR="dist/ardium-bundle-v2.5.5"
echo "🚀 [Major] Initiating Distribution Bundle for v2.5.5..."

# 1. Create Directory Structure
mkdir -p "$DIST_DIR/bin"
mkdir -p "$DIST_DIR/lib"
mkdir -p "$DIST_DIR/include"
mkdir -p "$DIST_DIR/stdlib"

# 2. Copy Compiler (arc)
echo "🔹 Packing Compiler (arc)..."
if [ -f "./arc" ]; then
    cp arc "$DIST_DIR/bin/"
else
    echo "⚠️ Warning: arc binary not found. Rebuilding..."
    clang++ src/cli/main.cpp -std=c++17 -O3 -o arc
    cp arc "$DIST_DIR/bin/"
fi

# 3. Copy Runtime (libTitan.a)
echo "🔹 Packing Runtime (libTitan.a)..."
if [ -f "libTitan.a" ]; then
    cp libTitan.a "$DIST_DIR/lib/"
else
    echo "⚠️ Warning: libTitan.a not found. Please run ./build_titan.sh first."
fi

# 4. Copy Headers
echo "🔹 Packing Headers..."
if [ -d "runtime/include" ]; then
    cp -R runtime/include/* "$DIST_DIR/include/"
fi

# 5. Copy Standard Library
echo "🔹 Packing Standard Library..."
if [ -d "stdlib" ]; then
    cp -R stdlib/* "$DIST_DIR/stdlib/"
fi

# 6. Create Version Info
echo "version=2.5.5" > "$DIST_DIR/VERSION"
echo "build_date=$(date)" >> "$DIST_DIR/VERSION"

# 7. Compress for distribution
echo "🔹 Compressing bundle..."
cd dist
tar -czf "ardium-v2.5.5-macos-arm64.tar.gz" "ardium-bundle-v2.5.5"
cd ..

echo "------------------------------------------------"
echo "✅ [Major] Bundle Complete: $DIST_DIR"
echo "   Archive: dist/ardium-v2.5.5-macos-arm64.tar.gz"
echo "------------------------------------------------"
