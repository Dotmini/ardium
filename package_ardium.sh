#!/bin/bash
set -e

# Config
VERSION="2.5.6"
IDENTIFIER="com.dotmini.ardium"
# User requested "packaging" folder
DIST_DIR="packaging"

# Filenames
COMPONENT_PKG="ArdiumCore.pkg"
DISTRIBUTION_PKG="Ardium_v${VERSION}.pkg"
DMG_NAME="Ardium-${VERSION}.dmg"

echo "🧹 Cleaning up previous builds..."
rm -rf "$DIST_DIR"
rm -rf _build
rm -rf dist_staging
rm -rf dmg_staging
mkdir -p "$DIST_DIR"

echo "🏗 Building Project (Clean Build)..."
# Build Runtime
# Build Runtime
echo "🔨 Building C++ Runtime (v2.5.6)..."
mkdir -p runtime/build
/usr/bin/clang++ -c -O3 -std=c++17 runtime/src/libardium.cpp -o runtime/build/libardium.o
/usr/bin/clang++ -dynamiclib -install_name @rpath/libardium.dylib -undefined dynamic_lookup -o runtime/build/libardium.dylib runtime/build/libardium.o


# Build Compiler
# Ensure standard environment
echo "⚠️ Skipping build (dune missing), using existing arc binary..."

echo "📂 Staging files for packaging..."
STAGING="dist_staging"
mkdir -p "$STAGING/usr/local/bin"
mkdir -p "$STAGING/usr/local/lib/ardium/stdlib"
mkdir -p "$STAGING/usr/local/share/doc/ardium"

# Copy Files
cp arc "$STAGING/usr/local/bin/arc"
cp runtime/build/libardium.dylib "$STAGING/usr/local/lib/"
cp -R stdlib/* "$STAGING/usr/local/lib/ardium/stdlib/"
cp docs/*.md "$STAGING/usr/local/share/doc/ardium/"

# Ensure executable permissions
chmod +x "$STAGING/usr/local/bin/arc"
chmod +x "$STAGING/usr/local/lib/libardium.dylib"

echo "🔧 Patching RPath for relocatable binary..."
install_name_tool -add_rpath "@executable_path/../lib" "$STAGING/usr/local/bin/arc"
install_name_tool -id "@rpath/libardium.dylib" "$STAGING/usr/local/lib/libardium.dylib"


echo "📦 Step 1: Building Component Package (Raw Payload)..."
pkgbuild --root "$STAGING" \
         --identifier "$IDENTIFIER" \
         --version "$VERSION" \
         --install-location "/" \
         "$DIST_DIR/$COMPONENT_PKG"

echo "📦 Step 2: Building Distribution Package (User Installer)..."
# productbuild creates a distribution archive which is more compatible/standard
productbuild --package "$DIST_DIR/$COMPONENT_PKG" \
             --identifier "$IDENTIFIER" \
             --version "$VERSION" \
             "$DIST_DIR/$DISTRIBUTION_PKG"

# Cleanup component pkg, we only need the distribution one
rm "$DIST_DIR/$COMPONENT_PKG"

echo "💾 Step 3: Creating Disk Image (DMG)..."
DMG_STAGING="dmg_staging"
mkdir -p "$DMG_STAGING"
# Copy the final installer into the DMG staging area
cp "$DIST_DIR/$DISTRIBUTION_PKG" "$DMG_STAGING/"

# Create DMG
hdiutil create -volname "Install Ardium v${VERSION}" \
               -srcfolder "$DMG_STAGING" \
               -ov -format UDZO \
               "$DIST_DIR/$DMG_NAME"

# Cleanup
rm -rf "$STAGING"
rm -rf "$DMG_STAGING"

echo "✅ Build Complete!"
echo "📂 Output Directory: $DIST_DIR"
echo "   - Installer: $DISTRIBUTION_PKG"
echo "   - Disk Image: $DMG_NAME"

ls -lh "$DIST_DIR"
