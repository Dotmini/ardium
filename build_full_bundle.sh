#!/bin/bash
set -e

VERSION="2.5.6"
IDENTIFIER="com.dotmini.ardium.full"
STAGING="full_bundle_staging"
DIST_DIR="dist"

echo "🚀 [Major] Initiating Full Bundle Master Build v$VERSION..."

# 1. Prepare Staging
rm -rf "$STAGING"
mkdir -p "$STAGING/usr/local/bin"
mkdir -p "$STAGING/usr/local/lib/ardium/stdlib"
mkdir -p "$STAGING/usr/local/include/ardium"
mkdir -p "$STAGING/usr/local/share/doc/ardium"

# 2. Re-compile Core Components
echo "🔨 Compiling Titan Engine & CLI..."
/usr/bin/clang++ runtime/src/TitanScript.cpp runtime/src/TitanCompiler.cpp runtime/src/TitanInterpreter.cpp -std=c++20 -O3 -Iruntime/include -o TitanScript
/usr/bin/clang++ linux/ardium_source/src/cli/main.cpp -std=c++17 -O3 -o arc

# 3. Harvest Binaries
cp arc "$STAGING/usr/local/bin/arc"
cp TitanScript "$STAGING/usr/local/bin/TitanScript"
chmod +x "$STAGING/usr/local/bin/arc"
chmod +x "$STAGING/usr/local/bin/TitanScript"

# 4. Harvest Libraries & Headers
[ -f "libTitan.a" ] && cp libTitan.a "$STAGING/usr/local/lib/"
[ -f "libardium.dylib" ] && cp libardium.dylib "$STAGING/usr/local/lib/"
cp -R runtime/include/* "$STAGING/usr/local/include/ardium/"

# 5. Harvest Stdlib & Docs
cp -R stdlib/* "$STAGING/usr/local/lib/ardium/stdlib/"
cp docs/*.md "$STAGING/usr/local/share/doc/ardium/" 2>/dev/null || true
cp *.md "$STAGING/usr/local/share/doc/ardium/" 2>/dev/null || true

# 6. Build Master PKG
echo "📦 Forging Master PKG..."
pkgbuild --root "$STAGING" \
         --identifier "$IDENTIFIER" \
         --version "$VERSION" \
         --install-location "/" \
         "$DIST_DIR/ArdiumFull_Internal.pkg"

productbuild --package "$DIST_DIR/ArdiumFull_Internal.pkg" \
             --identifier "$IDENTIFIER" \
             --version "$VERSION" \
             "$DIST_DIR/Ardium_v${VERSION}_Full.pkg"

# 7. Create DMG Wrapper
echo "💾 Wrapping in DMG..."
hdiutil create -volname "Ardium Full Bundle v$VERSION" \
               -srcfolder "$DIST_DIR/Ardium_v${VERSION}_Full.pkg" \
               -ov -format UDZO \
               "$DIST_DIR/Ardium_v${VERSION}_Full.dmg"

# 8. Cleanup
rm "$DIST_DIR/ArdiumFull_Internal.pkg"
rm -rf "$STAGING"

echo "✅ [Major] Full Bundle Complete: dist/Ardium_v${VERSION}_Full.dmg"
