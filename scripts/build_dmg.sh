#!/bin/bash
# build_dmg.sh - Create DMG disk image for Ardium

set -e

echo "💿 Building Ardium DMG"
echo "====================="

VERSION="2.0.0"
BUILD_DIR="build/release"
DMG_DIR="build/dmg"
DIST_DIR="dist"
DMG_NAME="Ardium-$VERSION"
VOLUME_NAME="Ardium $VERSION"

# Ensure build exists
if [ ! -d "$BUILD_DIR" ]; then
    echo "❌ Build directory not found. Run ./scripts/build_release.sh first"
    exit 1
fi

# Clean and create directories
rm -rf "$DMG_DIR"
mkdir -p "$DMG_DIR"
mkdir -p "$DIST_DIR"

# Create DMG staging directory
echo "📋 Preparing DMG contents..."
mkdir -p "$DMG_DIR/Ardium"
cp -r "$BUILD_DIR"/* "$DMG_DIR/Ardium/"

# Create README for DMG
cat > "$DMG_DIR/Ardium/READ ME.txt" << EOF
Ardium 2.0.0 - Native Programming Language for macOS
=====================================================

Installation:
1. Run ./install.sh
   OR
2. Manually copy 'ardium' to /usr/local/bin/

Quick Start:
   ardium run myprogram.ar

Documentation:
- README.md - Overview
- docs/API_REFERENCE.md - Complete API
- docs/QUICKSTART.md - Getting started
- examples/ - Example programs

Website: https://github.com/ardium (if applicable)

Enjoy coding with Ardium! 🚀
EOF

# Create Applications symlink (optional)
# ln -s /Applications "$DMG_DIR/Applications"

# Create temporary DMG
echo "🔨 Creating DMG..."
TMP_DMG="$DIST_DIR/tmp-$DMG_NAME.dmg"

hdiutil create \
    -volname "$VOLUME_NAME" \
    -srcfolder "$DMG_DIR/Ardium" \
    -ov \
    -format UDRW \
    "$TMP_DMG"

# Mount the temporary DMG
echo "📂 Mounting DMG..."
MOUNT_DIR=$(hdiutil attach "$TMP_DMG" | grep "Volumes" | awk '{print $3}')

# Optional: Set DMG appearance (requires Finder scripting)
# echo "🎨 Customizing DMG appearance..."
# You can add AppleScript here to customize the DMG window

# Unmount
echo "💿 Finalizing DMG..."
hdiutil detach "$MOUNT_DIR"

# Convert to compressed read-only DMG
FINAL_DMG="$DIST_DIR/$DMG_NAME.dmg"
hdiutil convert "$TMP_DMG" \
    -format UDZO \
    -o "$FINAL_DMG"

# Clean up
rm "$TMP_DMG"

echo ""
echo "✅ DMG created successfully!"
echo "💿 Location: $FINAL_DMG"
echo "💾 Size: $(du -h "$FINAL_DMG" | cut -f1)"
