# Ardium 2.0.0 - Distribution Guide

## 📦 Distribution Packages

Ardium 2.0.0 is available in two formats:

### PKG Installer

**File**: `Ardium-2.0.0.pkg`  
**Best for**: Easy installation, automatic PATH setup

**Installation**:

```bash
# Double-click or:
sudo installer -pkg Ardium-2.0.0.pkg -target /
```

**Installs to**: `/usr/local/ardium/`  
**Symlink created**: `/usr/local/bin/ardium` → `/usr/local/ardium/ardium`

**Includes**:

- Ardium compiler binary (121MB)
- Standard library (8 frameworks)
- Documentation
- Examples
- Runtime (runtime.m)

### DMG Disk Image

**File**: `Ardium-2.0.0.dmg`  
**Best for**: Manual installation, portable use

**Installation**:

1. Double-click `Ardium-2.0.0.dmg`
2. Mount the disk image
3. Copy `ardium` binary to `/usr/local/bin/` or desired location
4. Copy `stdlib/` folder to the same directory as `ardium`

**Manual installation**:

```bash
# Mount DMG
open Ardium-2.0.0.dmg

# Copy files
sudo cp -r /Volumes/Ardium\ 2.0.0/ardium /usr/local/bin/
sudo cp -r /Volumes/Ardium\ 2.0.0/stdlib /usr/local/ardium/

# Or use the included script
cd /Volumes/Ardium\ 2.0.0/
sudo ./install.sh
```

## ✅ Verification

After installation, verify Ardium is working:

```bash
# Check version
ardium --version  # Should show 2.0.0

# Run test
echo 'import "Core"
fn main() {
    println("Hello Ardium!")
    return 0
}' > test.ar

ardium run test.ar
# Expected output: Hello Ardium!
```

## 📁 Package Contents

Both packages include:

```
/usr/local/ardium/
├── ardium              # Compiler binary (121MB)
├── stdlib/             # Standard library
│   ├── Core.ar
│   ├── CoreUI.ar
│   ├── CoreAI.ar
│   ├── CoreData.ar
│   ├── CoreNetwork.ar
│   ├── CoreCrypto.ar
│   ├── CoreKits.ar
│   └── PlaygroundSupport.ar
├── runtime.m           # Native runtime
├── docs/               # Documentation
│   ├── API_REFERENCE.md
│   ├── QUICKSTART.md
│   └── BUILD_SUMMARY.md
├── examples/           # Example programs
└── README.md           # Overview

/usr/local/bin/
└── ardium -> /usr/local/ardium/ardium  # Symlink
```

## 🚀 Quick Start After Installation

```bash
# Hello World
echo 'import "Core"

fn main() {
    println("🚀 Ardium 2.0.0 works!")
    return 0
}' > hello.ar

ardium run hello.ar

# See documentation
cat /usr/local/ardium/docs/QUICKSTART.md

# Try examples
cd /usr/local/ardium/examples/
ardium run simple_button.ar  # GUI example
```

## 📊 Package Sizes

| Package | Size | Format |
|---------|------|--------|
| PKG | ~121MB | Installer |
| DMG | ~121MB | Disk Image |
| Binary only | 121MB | Executable |

*Size includes LLVM libraries and dependencies*

## 🗑️ Uninstallation

**PKG installation**:

```bash
sudo rm -rf /usr/local/ardium
sudo rm /usr/local/bin/ardium
```

**Manual installation**:

```bash
# Remove files you copied
sudo rm /usr/local/bin/ardium
sudo rm -rf /usr/local/ardium
```

## 🛠️ Developer Notes

### Building from Source

```bash
git clone <repo>
cd ardium
dune build --release
```

### Creating Packages

```bash
./scripts/build_all.sh
```

This creates:

- `dist/Ardium-2.0.0.pkg`
- `dist/Ardium-2.0.0.dmg`

## 📝 System Requirements

- **OS**: macOS 12.0 or later
- **Architecture**: arm64 (Apple Silicon) or x86_64 (Intel)
- **Disk Space**: 150MB minimum
- **Dependencies**: None (all bundled)

## 🔐 Code Signing

*Note: These packages are not currently code-signed. Users may need to allow execution in System Preferences → Security & Privacy.*

To bypass Gatekeeper (development only):

```bash
sudo xattr -rd com.apple.quarantine /usr/local/ardium/ardium
```

## 📄 License

MIT License - See README.md for details

---

**Support**: Check documentation in `/usr/local/ardium/docs/`  
**Version**: 2.0.0  
**Date**: 2026-01-02
