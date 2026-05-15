#!/bin/bash
# build_release.sh - Build Ardium 2.0.0 for distribution

set -e

echo "🚀 Building Ardium 2.0.0 Release Package"
echo "========================================"

# Configuration
VERSION="2.0.0"
BUILD_DIR="build/release"
DIST_DIR="dist"
APP_NAME="ardium"

# Clean previous builds
echo "🧹 Cleaning previous builds..."
rm -rf "$BUILD_DIR" "$DIST_DIR"
mkdir -p "$BUILD_DIR" "$DIST_DIR"

# Build the compiler
echo "🔨 Building Ardium compiler..."
unset AR  # Fix conflict with 'arc' command
dune clean
dune build --release

# Copy executable
echo "📦 Packaging executable..."
cp _build/default/bin/main.exe "$BUILD_DIR/$APP_NAME"
chmod +x "$BUILD_DIR/$APP_NAME"

# Copy stdlib
echo "📚 Copying standard library..."
cp -r stdlib "$BUILD_DIR/"

# Copy runtime
echo "⚙️  Copying runtime..."
cp lib/runtime.m "$BUILD_DIR/"

# Copy documentation
echo "📖 Copying documentation..."
mkdir -p "$BUILD_DIR/docs"
cp README.md "$BUILD_DIR/"
cp docs/API_REFERENCE.md "$BUILD_DIR/docs/"
cp docs/QUICKSTART.md "$BUILD_DIR/docs/"
cp BUILD_SUMMARY.md "$BUILD_DIR/"

# Copy examples
echo "💡 Copying examples..."
cp -r examples "$BUILD_DIR/"

# Create version file
echo "$VERSION" > "$BUILD_DIR/VERSION"

# Create install script
cat > "$BUILD_DIR/install.sh" << 'EOF'
#!/bin/bash
# Ardium installer script

INSTALL_DIR="/usr/local/ardium"
BIN_DIR="/usr/local/bin"

echo "Installing Ardium 2.0.0..."

# Create installation directory
sudo mkdir -p "$INSTALL_DIR"

# Copy files
sudo cp -r * "$INSTALL_DIR/"

# Create symlink
sudo ln -sf "$INSTALL_DIR/ardium" "$BIN_DIR/ardium"

echo "✅ Ardium installed successfully!"
echo "Run 'ardium --help' to get started"
EOF

chmod +x "$BUILD_DIR/install.sh"

# Test the build
echo "🧪 Testing build..."
"$BUILD_DIR/$APP_NAME" run tests/test_frameworks_basic.ar

echo ""
echo "✅ Build complete!"
echo "📁 Location: $BUILD_DIR"
echo "🎯 Next: Run ./build_pkg.sh and ./build_dmg.sh"
