#!/bin/bash
# build_pkg.sh - Create PKG installer for Ardium

set -e

echo "📦 Building Ardium PKG Installer"
echo "================================"

VERSION="2.0.0"
BUILD_DIR="build/release"
PKG_DIR="build/pkg"
DIST_DIR="dist"

# Ensure build exists
if [ ! -d "$BUILD_DIR" ]; then
    echo "❌ Build directory not found. Run ./scripts/build_release.sh first"
    exit 1
fi

# Clean and create directories
rm -rf "$PKG_DIR"
mkdir -p "$PKG_DIR/root/usr/local/ardium"
mkdir -p "$PKG_DIR/scripts"
mkdir -p "$DIST_DIR"

# Copy files to package root
echo "📋 Preparing package contents..."
cp -r "$BUILD_DIR"/* "$PKG_DIR/root/usr/local/ardium/"

# Create postinstall script
cat > "$PKG_DIR/scripts/postinstall" << 'EOF'
#!/bin/bash
# Postinstall script

BIN_DIR="/usr/local/bin"
INSTALL_DIR="/usr/local/ardium"

# Create symlink
ln -sf "$INSTALL_DIR/ardium" "$BIN_DIR/ardium"

# Set permissions
chmod -R 755 "$INSTALL_DIR"
chmod +x "$INSTALL_DIR/ardium"

echo "Ardium 2.0.0 installed successfully!"
exit 0
EOF

chmod +x "$PKG_DIR/scripts/postinstall"

# Build the package
echo "🔨 Building PKG..."
pkgbuild \
    --root "$PKG_DIR/root" \
    --scripts "$PKG_DIR/scripts" \
    --identifier "com.ardium.compiler" \
    --version "$VERSION" \
    --install-location / \
    "$DIST_DIR/Ardium-$VERSION.pkg"

# Create product archive with metadata
productbuild \
    --package "$DIST_DIR/Ardium-$VERSION.pkg" \
    --version "$VERSION" \
    "$DIST_DIR/Ardium-$VERSION-Installer.pkg"

# Clean up intermediate package
rm "$DIST_DIR/Ardium-$VERSION.pkg"
mv "$DIST_DIR/Ardium-$VERSION-Installer.pkg" "$DIST_DIR/Ardium-$VERSION.pkg"

echo ""
echo "✅ PKG created successfully!"
echo "📦 Location: $DIST_DIR/Ardium-$VERSION.pkg"
echo "💾 Size: $(du -h "$DIST_DIR/Ardium-$VERSION.pkg" | cut -f1)"
