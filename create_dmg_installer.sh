#!/bin/bash
set -e

# ==============================================================================
#  ARDIUM v2.5.5 ENTERPRISE DMG/PKG BUILDER
# ==============================================================================
#  Architect: Major
#  Branding: Dotmini Software Master Distribution
# ==============================================================================

VERSION="2.6.0"
PKG_NAME="Ardium_v${VERSION}.pkg"
DMG_NAME="Ardium_v${VERSION}.dmg"
IDENTIFIER="com.dotmini.ardium"
STAGING_ROOT="packaging/root"

echo "🚀 [Major] Initiating Master Distribution Pipeline..."

# 1. Clear and Prepare Staging
echo "🔹 Sanitizing staging environment..."
rm -rf "$STAGING_ROOT"
mkdir -p "$STAGING_ROOT/usr/local/bin"
mkdir -p "$STAGING_ROOT/usr/local/lib/ardium"
mkdir -p "$STAGING_ROOT/usr/local/include/ardium"

# 2. Collect Binaries
echo "🔹 Harvesting binaries..."
if [ -f "./arc" ]; then
    cp arc "$STAGING_ROOT/usr/local/bin/"
    chmod +x "$STAGING_ROOT/usr/local/bin/arc"
fi

# IMPORTANT: Include the 114MB OCaml Backend
if [ -f "_build/default/bin/main.exe" ]; then
    cp _build/default/bin/main.exe "$STAGING_ROOT/usr/local/bin/ardium-backend"
    chmod +x "$STAGING_ROOT/usr/local/bin/ardium-backend"
    echo "✅ Backend integrated (114MB)."
fi

# Include the Script Engine (Titan VM)
if [ -f "./TitanScript" ]; then
    cp TitanScript "$STAGING_ROOT/usr/local/bin/"
    chmod +x "$STAGING_ROOT/usr/local/bin/TitanScript"
    echo "✅ Script Engine integrated."
fi

# 3. Collect Runtime Library
echo "🔹 Harvesting runtime..."
if [ -f "libTitan.a" ]; then
    cp libTitan.a "$STAGING_ROOT/usr/local/lib/libTitan.a"
fi

# 4. Collect Headers
echo "🔹 Harvesting headers..."
if [ -d "runtime/include" ]; then
    cp -R runtime/include/* "$STAGING_ROOT/usr/local/include/ardium/"
fi

# 5. Collect Standard Library
echo "🔹 Harvesting stdlib..."
if [ -d "stdlib" ]; then
    cp -R stdlib/ "$STAGING_ROOT/usr/local/lib/ardium/stdlib"
fi

# 6. Build Component Package
echo "🔹 Building master component package..."
pkgbuild --root "$STAGING_ROOT" \
         --identifier "$IDENTIFIER" \
         --version "$VERSION" \
         --install-location "/" \
         packaging/ArdiumMaster.pkg

# 7. Generate Master Distribution XML
echo "🔹 Generating master distribution control..."
cat <<EOF > packaging/master_distribution.xml
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="1">
    <title>Ardium v$VERSION - Titan Engine</title>
    <options customize="never" require-scripts="false"/>
    <background file="background.png" alignment="bottomleft" scaling="none"/>
    <welcome file="Welcome.html" mime-type="text/html"/>
    <license file="License.txt" mime-type="text/plain"/>
    <choices-outline>
        <line choice="default"/>
    </choices-outline>
    <choice id="default" visible="false">
        <pkg-ref id="$IDENTIFIER"/>
    </choice>
    <pkg-ref id="$IDENTIFIER" version="$VERSION" onConclusion="none">ArdiumMaster.pkg</pkg-ref>
</installer-gui-script>
EOF

# 8. Finalize Product Package
echo "🔹 Finalizing master installer (.pkg)..."
productbuild --distribution packaging/master_distribution.xml \
             --resources packaging/resources \
             --package-path packaging \
             "dist/$PKG_NAME"

# 9. Create DMG Wrapper
echo "🔹 Forging DMG container..."

# Try to use create-dmg if available for beautiful layout
if command -v create-dmg &> /dev/null; then
    create-dmg \
        --volname "Ardium v$VERSION" \
        --background "packaging/resources/background.png" \
        --window-pos 200 120 \
        --window-size 800 400 \
        --icon-size 100 \
        --icon "$PKG_NAME" 400 200 \
        --hide-extension "$PKG_NAME" \
        --app-drop-link 600 200 \
        "dist/$DMG_NAME" \
        "dist/$PKG_NAME" || hdiutil create -volname "Ardium v$VERSION" -srcfolder "dist/$PKG_NAME" -ov -format UDZO "dist/$DMG_NAME"
else
    hdiutil create -volname "Ardium v$VERSION" \
                   -srcfolder "dist/$PKG_NAME" \
                   -ov -format UDZO \
                   "dist/$DMG_NAME"
fi

# 10. Cleanup
rm packaging/ArdiumMaster.pkg
rm packaging/master_distribution.xml

echo "------------------------------------------------"
echo "✅ [Major] Master Distribution Ready."
echo "   PKG: dist/$PKG_NAME"
echo "   DMG: dist/$DMG_NAME"
echo "   Status: DEPLOYMENT READY"
echo "------------------------------------------------"
