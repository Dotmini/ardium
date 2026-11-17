#!/bin/bash
set -e

# Configuration
VERSION="3.0.0"
IDENTIFIER="com.dotmini.arc"
PKG_NAME="Arc_v${VERSION}.pkg"
DMG_NAME="Arc_v${VERSION}.dmg"
STAGING="dist_staging"
DIST_DIR="dist"

echo "🚀 Starting Final Build for Arc Language v$VERSION..."

# 1. Cleanup
rm -rf "$STAGING" "$DIST_DIR"
mkdir -p "$STAGING/usr/local/bin"
mkdir -p "$STAGING/usr/local/lib/arc/stdlib"
mkdir -p "$STAGING/usr/local/lib/ardium/stdlib"
mkdir -p "$DIST_DIR"

# 2. Build Compiler (TitanScript)
echo "🔨 Building TitanScript (Arc Native Engine)..."
/usr/bin/clang++ runtime/src/TitanScript.cpp runtime/src/TitanCompiler.cpp runtime/src/TitanInterpreter.cpp -std=c++20 -O3 -Iruntime/include -o TitanScript

# 3. Build CLI (arc)
echo "🔨 Building ARC CLI..."
make build

# 4. Build Runtime Libraries
echo "🔨 Building Arc Runtime Libraries..."
bash runtime/build_runtime.sh || true
if [ -f "runtime/build/libardium.dylib" ]; then
    cp runtime/build/libardium.dylib runtime/build/libarc.dylib 2>/dev/null || true
    mkdir -p "$STAGING/usr/local/lib"
    cp runtime/build/libardium.dylib "$STAGING/usr/local/lib/"
    cp runtime/build/libardium.dylib "$STAGING/usr/local/lib/libarc.dylib"
fi

# 5. Stage Binaries
echo "📂 Staging binaries..."
cp arc "$STAGING/usr/local/bin/arc"
cp TitanScript "$STAGING/usr/local/bin/TitanScript"
chmod +x "$STAGING/usr/local/bin/arc"
chmod +x "$STAGING/usr/local/bin/TitanScript"

# 6. Stage Standard Library
echo "📂 Staging stdlib..."
if [ -d "stdlib" ]; then
    cp -R stdlib/* "$STAGING/usr/local/lib/arc/stdlib/" 2>/dev/null || true
    cp -R stdlib/* "$STAGING/usr/local/lib/ardium/stdlib/" 2>/dev/null || true
fi

# 7. Build Component Package
echo "📦 Building Component Package..."
pkgbuild --root "$STAGING" \
         --identifier "$IDENTIFIER" \
         --version "$VERSION" \
         --install-location "/" \
         "$DIST_DIR/ArcCore.pkg"

# 8. Build Product Package (Simple Distribution)
echo "📦 Building Product Package..."
productbuild --package "$DIST_DIR/ArcCore.pkg" \
             --identifier "$IDENTIFIER" \
             --version "$VERSION" \
             "$DIST_DIR/$PKG_NAME"

# 9. Create DMG
echo "💾 Creating Disk Image (DMG)..."
hdiutil create -volname "Arc Language v$VERSION" \
               -srcfolder "$DIST_DIR/$PKG_NAME" \
               -ov -format UDZO \
               "$DIST_DIR/$DMG_NAME"

# 10. Cleanup Intermediate
rm -f "$DIST_DIR/ArcCore.pkg"
rm -rf "$STAGING"

echo "✅ Build Complete!"
echo "   PKG: $DIST_DIR/$PKG_NAME"
echo "   DMG: $DIST_DIR/$DMG_NAME"

