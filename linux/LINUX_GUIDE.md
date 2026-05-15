# 🐧 Ardium Linux Guide

This document explains how to build, install, and use Ardium on Linux systems.

## 📋 Prerequisites

To build Ardium on Linux, you need:

- **OCaml & Dune**: For compiling the compiler source.
- **GCC**: For linking the runtime.
- **rpm-build**: For creating the RPM package (RedHat/Fedora/OpenSUSE).

### Installing Dependencies (Fedora/RHEL)

```bash
sudo dnf install ocaml dune gcc rpm-build
```

### Installing Dependencies (Ubuntu/Debian)

*Note: RPM build is not native to Debian-based systems, but you can compile from source.*

```bash
sudo apt install ocaml dune gcc
```

---

## 🛠 Building the RPM Package

We have provided a convenient script to build the `.rpm` package.

1. Navigate to the `linux` folder:

   ```bash
   cd linux
   ```

2. Run the build script:

   ```bash
   chmod +x build_rpm.sh
   ./build_rpm.sh
   ```

3. The output package will be in `linux/rpmbuild/RPMS/`.

---

## 💿 Installing Ardium

### From RPM

```bash
sudo rpm -ivh ardium-2.5.0.2-1.aarch64.rpm
```

### From Source

If you are not using an RPM-based system:

```bash
# In the root directory
make build
sudo cp _build/default/bin/main.exe /usr/local/bin/arc
```

---

## 🚀 Running Ardium on Linux

Once installed, usage is identical to macOS:

```bash
# Verify installation
arc --version

# Run a file
arc run hello.ar
```

## ⚠️ Linux-Specific Notes

- **GUI Limitations**: The MacOS-native GUI functions (`create_apple_window`, etc.) are **NOT** available on Linux.
- **Runtime**: Ardium on Linux uses the standard libc runtime.

## 📦 Building AppImage (Portable)

You can also build a portable `.AppImage` that runs on most Linux distros.

1. Install `wget` (to download appimagetool).
2. Run:

   ```bash
   chmod +x build_appimage.sh
   ./build_appimage.sh
   ```

3. The result will be `Ardium-x86_64.AppImage`.

---
(C) 2026 Arsenal Engine Project
