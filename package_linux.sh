#!/bin/bash
set -e

VERSION="3.0.0"
ARCH="amd64"
PKG_NAME="arc-lang_${VERSION}_${ARCH}"
DIST_DIR="dist/linux"

echo "🐧 Building Arc Language Linux Server Distribution..."
rm -rf "$DIST_DIR"
mkdir -p "$DIST_DIR/$PKG_NAME/DEBIAN"
mkdir -p "$DIST_DIR/$PKG_NAME/usr/local/bin"
mkdir -p "$DIST_DIR/$PKG_NAME/usr/local/lib/arc/stdlib"

# Copy binaries
cp arc "$DIST_DIR/$PKG_NAME/usr/local/bin/" 2>/dev/null || true
cp TitanScript "$DIST_DIR/$PKG_NAME/usr/local/bin/" 2>/dev/null || true

# Copy stdlib
if [ -d "stdlib" ]; then
    cp -R stdlib/* "$DIST_DIR/$PKG_NAME/usr/local/lib/arc/stdlib/"
fi

# Create Debian control file
cat << CONTROL_EOF > "$DIST_DIR/$PKG_NAME/DEBIAN/control"
Package: arc-lang
Version: $VERSION
Section: devel
Priority: optional
Architecture: $ARCH
Maintainer: Tirawat Nantamas <team@dotmini.org>
Description: Arc Programming Language Compiler & Runtime
 High-performance systems and backend programming language for Linux and macOS.
CONTROL_EOF

echo "📝 Package metadata staged at: $DIST_DIR/$PKG_NAME"
if command -v dpkg-deb &> /dev/null; then
    dpkg-deb --build "$DIST_DIR/$PKG_NAME"
    echo "✅ Created: $DIST_DIR/${PKG_NAME}.deb"
else
    # Create tarball if dpkg-deb not present (e.g. on macOS)
    cd "$DIST_DIR"
    tar -czf "${PKG_NAME}.tar.gz" "$PKG_NAME"
    cd - > /dev/null
    echo "✅ Created Linux Tarball: $DIST_DIR/${PKG_NAME}.tar.gz"
fi
