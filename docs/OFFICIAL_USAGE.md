# 📘 Official Ardium Usage Guide

This document provides the official guidelines for using the Ardium programming language, running tests, and packaging applications.

## 🚀 Quick Start

### Prerequisites

- macOS (ARM64 recommended)
- `make` and `dune` (for compiler development)
- `pkgbuild` (for packaging)

### Basic Commands

The Ardium CLI (`arc`) is your main tool.

```bash
# Run a script directly
arc run main.ar

# Build an optimized executable
arc build main.ar -o myapp

# Run integration tests
arc test tests/*.ar

# Show version
arc --version
```

---

## 🛠 Language Features

Ardium is designed for **flexibility** and **performance**.

### 1. Flexible Typing

Ardium uses a hybrid static-dynamic approach.

```rust
let x = 10;          // Int
let name = "Hello";  // String
let mutable_var = 1; // Immutable by default, use 'let mut' for mutable
```

### 2. Native Interop (FFI)

Easily call C functions or MacOS Frameworks.

```rust
extern fn printf(fmt, ...);
extern fn create_apple_window(title, x, y, w, h);

fn main() {
    printf("Hello Native World!\n");
    create_apple_window("Ardium App", 0, 0, 800, 600);
}
```

### 3. Layout Macros

Use declarative decorators for UI.

```rust
@VClass
fn MainView() {
    // Items here are vertically stacked
}
```

---

## ✅ Running Tests

To verify the compiler and runtime stability:

```bash
# Run the full test suite
make test

# Run a specific test
arc run test_layout.ar
```

---

## 📦 Packaging and Distribution

### macOS (PKG & DMG)

We provide an automated script to build professional installers.

```bash
# Build .pkg and .dmg
./package_ardium.sh
```

**Output**:

- `packaging/Ardium_v{version}.pkg`: Installer package
- `packaging/Ardium-{version}.dmg`: Disk Image

### Linux (RPM)

To build an RPM package for Linux distributions (RedHat, Fedora, SUSE):

1. Ensure `rpmbuild` is installed (`sudo dnf install rpm-build`).
2. Create the package using the spec file (see `packaging/ardium.spec`).
3. Run: `rpmbuild -bb packaging/ardium.spec`

*Note: RPM building requires a Linux environment or a cross-compilation toolchain.*
