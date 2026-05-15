#!/bin/bash
set -e

# Configuration
VERSION="2.5.6"
IDENTIFIER="com.dotmini.ardium"
PKG_NAME="Ardium_v${VERSION}.pkg"
DMG_NAME="Ardium_v${VERSION}.dmg"
STAGING="dist_staging"
DIST_DIR="dist"

echo "🚀 Starting Final Build for Ardium v$VERSION..."

# 1. Cleanup
rm -rf "$STAGING" "$DIST_DIR"
mkdir -p "$STAGING/usr/local/bin"
mkdir -p "$STAGING/usr/local/lib/ardium/stdlib"
mkdir -p "$DIST_DIR"

# 2. Build Compiler (TitanScript)
echo "🔨 Building TitanScript (Native C++)..."
/usr/bin/clang++ runtime/src/TitanScript.cpp runtime/src/TitanCompiler.cpp runtime/src/TitanInterpreter.cpp -std=c++20 -O3 -Iruntime/include -o TitanScript

# 3. Build CLI (arc)
echo "🔨 Building ARC CLI..."
/usr/bin/clang++ linux/ardium_source/src/cli/main.cpp -std=c++17 -O3 -o arc

# 4. Stage Binaries
echo "📂 Staging binaries..."
cp arc "$STAGING/usr/local/bin/arc"
cp TitanScript "$STAGING/usr/local/bin/TitanScript"
chmod +x "$STAGING/usr/local/bin/arc"
chmod +x "$STAGING/usr/local/bin/TitanScript"

# 5. Stage Standard Library
echo "📂 Staging stdlib..."
cp -R stdlib/* "$STAGING/usr/local/lib/ardium/stdlib/"

# 5. Build Component Package
echo "📦 Building Component Package..."
pkgbuild --root "$STAGING" \
         --identifier "$IDENTIFIER" \
         --version "$VERSION" \
         --install-location "/" \
         "$DIST_DIR/ArdiumCore.pkg"

# 6. Build Product Package (Simple Distribution)
echo "📦 Building Product Package..."
productbuild --package "$DIST_DIR/ArdiumCore.pkg" \
             --identifier "$IDENTIFIER" \
             --version "$VERSION" \
             "$DIST_DIR/$PKG_NAME"

# 7. Create DMG
echo "💾 Creating Disk Image (DMG)..."
hdiutil create -volname "Ardium v$VERSION" \
               -srcfolder "$DIST_DIR/$PKG_NAME" \
               -ov -format UDZO \
               "$DIST_DIR/$DMG_NAME"

# 8. Cleanup Intermediate
rm "$DIST_DIR/ArdiumCore.pkg"
rm -rf "$STAGING"

echo "✅ Build Complete!"
echo "   PKG: $DIST_DIR/$PKG_NAME"
echo "   DMG: $DIST_DIR/$DMG_NAME"
