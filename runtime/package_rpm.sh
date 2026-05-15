#!/bin/bash
set -e

# Configuration
VERSION="2.0.0"
PACKAGE_NAME="ardium-runtime"
BUILD_DIR="runtime/build"
RPM_ROOT="$BUILD_DIR/rpmbuild"

# OS Check
OS=$(uname -s)
if [ "$OS" != "Linux" ]; then
    echo "⚠️  Warning: You are running this on $OS."
    echo "   RPM creation requires 'rpmbuild' which is typically on Linux."
fi

# Ensure Runtime is Built
echo "🔨 Ensuring Runtime is built..."
./runtime/build_runtime.sh

if [ ! -f "$BUILD_DIR/libardium.so" ]; then
    echo "❌ Error: runtime/build/libardium.so not found."
    exit 1
fi

echo "📦 Setting up RPM Build Environment..."
rm -rf "$RPM_ROOT"
mkdir -p "$RPM_ROOT"/{BUILD,RPMS,SOURCES,SPECS,SRPMS}

# Create Spec File
echo "📝 Generating SPEC File..."
cat > "$RPM_ROOT/SPECS/ardium.spec" <<EOF
Name:       $PACKAGE_NAME
Version:    $VERSION
Release:    1%{?dist}
Summary:    Ardium Language Runtime
License:    MIT
URL:        https://ardium-lang.org
BuildArch:  x86_64

%description
The shared runtime library for the Ardium programming language.
Supports RHEL/CentOS/Fedora systems.

%install
mkdir -p %{buildroot}/usr/lib64
install -m 755 $PWD/$BUILD_DIR/libardium.so %{buildroot}/usr/lib64/libardium.so

%files
/usr/lib64/libardium.so

%post
ldconfig

%postun
ldconfig
EOF

# Build RPM
echo "🔨 Building .rpm package..."
# Note: In a real env, we'd use rpmbuild -bb. 
# We simulate the command output for the user.
echo "   Command: rpmbuild -bb --define \"_topdir $PWD/$RPM_ROOT\" $RPM_ROOT/SPECS/ardium.spec"

# Only try to run if rpmbuild exists
if command -v rpmbuild &> /dev/null; then
    rpmbuild -bb --define "_topdir $PWD/$RPM_ROOT" "$RPM_ROOT/SPECS/ardium.spec"
    echo "✅ RPM Created in $RPM_ROOT/RPMS/x86_64/"
else
    echo "⚠️  'rpmbuild' not found. This script generates the SPEC file but requires rpmbuild to finish."
    echo "   SPEC file created at: $RPM_ROOT/SPECS/ardium.spec"
fi
