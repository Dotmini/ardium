#!/bin/bash
set -e

# ==============================================================================
#  ARC STUDIO IDE BUILDER
# ==============================================================================

APP_NAME="ArcStudio"
BUNDLE_ID="com.dotmini.arc.studio"
VERSION="3.0.0"
SRC_DIR="."
BUILD_DIR="build"
APP_BUNDLE="$BUILD_DIR/$APP_NAME.app"
MACOS_DIR="$APP_BUNDLE/Contents/MacOS"
RESOURCES_DIR="$APP_BUNDLE/Contents/Resources"

echo "🚀 Initiating Arc Studio IDE Build..."

# 1. Clean Workspace
rm -rf "$BUILD_DIR"
mkdir -p "$MACOS_DIR"
mkdir -p "$RESOURCES_DIR"

# 2. Compile Sources
echo "🔹 Compiling Swift Sources..."
swiftc "$SRC_DIR/ArcStudioApp.swift" \
       "$SRC_DIR/ContentView.swift" \
       "$SRC_DIR/ArcEditor.swift" \
       "$SRC_DIR/ConsoleView.swift" \
       -o "$MACOS_DIR/$APP_NAME" \
       -target arm64-apple-macosx13.0 \
       -O

echo "✅ Binary Compiled."

# 3. Create Info.plist
echo "🔹 Generating Info.plist..."
cat > "$APP_BUNDLE/Contents/Info.plist" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleExecutable</key>
    <string>$APP_NAME</string>
    <key>CFBundleIdentifier</key>
    <string>$BUNDLE_ID</string>
    <key>CFBundleName</key>
    <string>$APP_NAME</string>
    <key>CFBundleShortVersionString</key>
    <string>$VERSION</string>
    <key>CFBundleVersion</key>
    <string>1</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>LSMinimumSystemVersion</key>
    <string>13.0</string>
    <key>NSHighResolutionCapable</key>
    <true/>
    <key>NSPrincipalClass</key>
    <string>NSApplication</string>
</dict>
</plist>
EOF

# 4. Sign the App
echo "🔹 Signing Application..."
codesign --force --deep --sign - "$APP_BUNDLE"

# 5. Build Installer Package (.pkg)
echo "🔹 Building Installer Package..."
PKG_OUTPUT="$BUILD_DIR/${APP_NAME}_v${VERSION}.pkg"

pkgbuild --root "$APP_BUNDLE" \
         --identifier "$BUNDLE_ID" \
         --version "$VERSION" \
         --install-location "/Applications/$APP_NAME.app" \
         "$PKG_OUTPUT"

echo "------------------------------------------------"
echo "✅ Build Complete."
echo "   App Bundle: $APP_BUNDLE"
echo "   Installer:  $PKG_OUTPUT"
echo "------------------------------------------------"

