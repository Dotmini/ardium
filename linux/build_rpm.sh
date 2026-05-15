#!/bin/bash
set -e

# Ardium Linux RPM Builder Script
# Usage: ./build_rpm.sh

VERSION="2.5.0.2"
echo "🐧 Building Ardium v$VERSION for Linux (RPM)..."

# Ensure we have the spec file
if [ ! -f "ardium.spec" ]; then
    echo "❌ Error: ardium.spec not found in clean folder."
    exit 1
fi

# Check for rpmbuild
if ! command -v rpmbuild &> /dev/null; then
    echo "❌ Error: 'rpmbuild' is not installed."
    echo "   Please install it (e.g., sudo dnf install rpm-build)"
    exit 1
fi

# Prepare Build Directory Structure for rpmbuild
echo "📂 Setting up build directories..."
mkdir -p rpmbuild/{BUILD,RPMS,SOURCES,SPECS,SRPMS}

# Create Source Tarball
echo "📦 Creating source tarball..."
# We need to package the source from the root directory
# Assuming this script is run from the 'linux' folder, we go up one level
cd ..
tar --exclude='_build' --exclude='.git' --exclude='node_modules' -czf linux/rpmbuild/SOURCES/ardium-$VERSION.tar.gz .
cd linux

# Copy Spec File
cp ardium.spec rpmbuild/SPECS/

# Build RPM
echo "🔨 Running rpmbuild..."
rpmbuild --define "_topdir $(pwd)/rpmbuild" -bb rpmbuild/SPECS/ardium.spec

echo "✅ Build Complete!"
echo "📍 RPM Package located at: $(pwd)/rpmbuild/RPMS/"
