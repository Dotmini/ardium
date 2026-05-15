#!/bin/bash
# build_all.sh - Build all distribution packages

set -e

echo "🎯 Ardium 2.0.0 - Complete Build Pipeline"
echo "=========================================="
echo ""

# Step 1: Build release
echo "Step 1/3: Building release..."
./scripts/build_release.sh

echo ""
echo "=========================================="
echo ""

# Step 2: Build PKG
echo "Step 2/3: Creating PKG installer..."
./scripts/build_pkg.sh

echo ""
echo "=========================================="
echo ""

# Step 3: Build DMG
echo "Step 3/3: Creating DMG..."
./scripts/build_dmg.sh

echo ""
echo "=========================================="
echo "🎉 All packages built successfully!"
echo "=========================================="
echo ""
echo "Distribution files:"
ls -lh dist/
echo ""
echo "Ready for distribution! 🚀"
