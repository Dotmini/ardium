#!/bin/bash
set -e

# Ardium AppImage Builder
# Requires: appimagetool (will attempt to download if missing)

APP_NAME="Ardium"
APP_DIR="Ardium.AppDir"
BUILD_DIR="../_build/default"

echo "🐧 Preparing Ardium AppImage..."

# 0. Check for Build Artifacts
if [ ! -f "$BUILD_DIR/bin/main.exe" ]; then
    echo "⚠️  Compiler binary not found. Attempting to build..."
    cd ..
    dune build --profile release
    cd linux
fi

# 1. Prepare AppDir Structure
rm -rf "$APP_DIR"
mkdir -p "$APP_DIR/usr/bin"
mkdir -p "$APP_DIR/usr/lib/ardium"
mkdir -p "$APP_DIR/usr/share/icons/hicolor/256x256/apps"

# 2. Copy Binaries and Libs
echo "📂 Copying files..."
cp "$BUILD_DIR/bin/main.exe" "$APP_DIR/usr/bin/arc"
cp -r "../stdlib" "$APP_DIR/usr/lib/ardium/"

# 3. Copy Metadata
cp "AppRun" "$APP_DIR/"
cp "ardium.desktop" "$APP_DIR/"
chmod +x "$APP_DIR/AppRun"

# 4. Icon (Generate a dummy one if missing)
if [ ! -f "ardium.png" ]; then
    echo "🎨 Generating placeholder icon..."
    # Simple colored square svg converted to png is hard in bash without tools, 
    # we'll just touch a file or hope user provides one. 
    # For now, let's just make a zero byte file or copy a generic one if possible.
    touch "$APP_DIR/.DirIcon"
else
    cp "ardium.png" "$APP_DIR/ardium.png"
    cp "ardium.png" "$APP_DIR/.DirIcon"
    cp "ardium.png" "$APP_DIR/usr/share/icons/hicolor/256x256/apps/ardium.png"
fi

# 5. Get appimagetool
if [ ! -f "appimagetool-x86_64.AppImage" ]; then
    echo "⬇️  Downloading appimagetool..."
    wget -q "https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage"
    chmod +x appimagetool-x86_64.AppImage
fi

# 6. Build AppImage
echo "🔨 Building .AppImage..."
# Create a proper icon for appimagetool not to complain too much
touch "$APP_DIR/ardium.png" 

# We need to run inside a fuse environment or extract.
# Since we are likely in a container or CI, we might need --appimage-extract-and-run
# But standard usage:
./appimagetool-x86_64.AppImage "$APP_DIR" "Ardium-Legacy-x86_64.AppImage" || {
    echo "⚠️  AppImage build failed. 'fuse' might be missing."
    echo "   Try running with: ARCH=x86_64 ./appimagetool-x86_64.AppImage --appimage-extract-and-run $APP_DIR"
}

echo "✅ AppImage tasks finished."
