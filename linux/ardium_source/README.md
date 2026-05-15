# Ardium 2.0.0 "Ascension" 🚀

A modern, native programming language for macOS with SwiftUI-inspired declarative UI and comprehensive framework ecosystem.

## ✨ Features

- **🎨 Declarative UI**: SwiftUI-like layout system (VStack, HStack, ZStack)
- **🧠 AI/ML Support**: Hardware-accelerated matrix operations via Accelerate framework
- **🌐 Networking**: Built-in HTTP client
- **🔐 Cryptography**: SHA-256 hashing and security primitives
- **💾 Data Persistence**: File I/O and data storage
- **⚡ High Performance**: Direct LLVM compilation to native code
- **🔧 Self-Hosting Ready**: Two-pass compilation for forward references

## 🎯 Quick Start

### Prerequisites

- macOS 12.0 or later
- OCaml 4.14+ with Dune
- LLVM 14+
- Xcode Command Line Tools

### Installation

```bash
# Clone the repository
git clone <repo-url>
cd ardium

# Build the compiler
dune build

# Run your first program
dune exec -- ardium run examples/hello.ar
```

### Hello World

```ardium
import "Core"

fn main() {
    println("Hello, Ardium 2.0! 🌟")
    return 0
}
```

### GUI Application

```ardium
import "Core"
import "CoreUI"

fn on_click() {
    println("Button clicked!")
}

fn build_ui() {
    VStack(resolve_symbol("content"))
}

fn content() {
    Title("Welcome to Ardium")
    Subtitle("Version 2.0 Ascension")
    Spacer(20)
    Button("Click Me", resolve_symbol("on_click"))
}

fn main() {
    App("My First App", resolve_symbol("build_ui"))
    return 0
}
```

## 📚 Documentation

- **[API Reference](docs/API_REFERENCE.md)** - Complete framework documentation
- **[Walkthrough](../../.gemini/antigravity/brain/11a28a47-60fd-4dcf-a562-0f893f8592b9/walkthrough.md)** - Implementation details

## 🏗️ Framework Ecosystem

Ardium 2.0.0 includes **8 comprehensive frameworks**:

| Framework | Purpose | Key Features |
|-----------|---------|-------------|
| **Core** | Fundamentals | `print`, `println`, basic I/O |
| **CoreUI** | GUI Development | VStack, HStack, Button, TextField, Image |
| **CoreAI** | Machine Learning | Matrix multiplication, Tensor operations |
| **CoreData** | Persistence | File I/O (`Load`, `Save`) |
| **CoreNetwork** | Networking | HTTP client (`Fetch`) |
| **CoreCrypto** | Security | SHA-256 hashing |
| **CoreKits** | Utilities | Logging, data structures |
| **PlaygroundSupport** | Development | Hot-reloading, debugging |

## 🛠️ Build Commands

```bash
# Clean build
dune clean && dune build

# Run a program
dune exec -- ardium run myapp.ar

# Compile to executable
dune exec -- ardium build myapp.ar -o myapp
./myapp

# Run tests
dune exec -- ardium run tests/test_frameworks_basic.ar
```

## 📁 Project Structure

```
ardium/
├── bin/              # Compiler entry point
│   └── main.ml
├── lib/              # Compiler implementation
│   ├── lexer.mll     # Lexical analyzer
│   ├── parser.mly    # Parser
│   ├── codegen.ml    # LLVM code generation
│   └── runtime.m     # Native Objective-C runtime
├── stdlib/           # Standard library frameworks
│   ├── Core.ar
│   ├── CoreUI.ar
│   ├── CoreAI.ar
│   ├── CoreData.ar
│   ├── CoreNetwork.ar
│   ├── CoreCrypto.ar
│   ├── CoreKits.ar
│   └── PlaygroundSupport.ar
├── tests/            # Test suite
└── docs/             # Documentation
```

## 🧪 Testing

```bash
# Run basic framework test
dune exec -- ardium run tests/test_frameworks_basic.ar

# Run specific test
dune exec -- ardium run tests/test_import.ar

# Expected output:
# 🚀 Ardium 2.0.0 Core Framework Test
# Testing print: ✓ Works!
# 🎉 Core framework operational!
```

## 🎨 Example Programs

### File I/O

```ardium
import "Core"
import "CoreData"

fn main() {
    Save("/tmp/message.txt", "Hello from Ardium!")
    let content = Load("/tmp/message.txt")
    println(content)
    return 0
}
```

### Networking

```ardium
import "Core"
import "CoreNetwork"

fn main() {
    println("Fetching...")
    let response = Fetch("https://httpbin.org/get")
    println(response)
    return 0
}
```

### Cryptography

```ardium
import "Core"
import "CoreCrypto"

fn main() {
    let hash = SHA256("password123")
    println("Hash: " + hash)
    return 0
}
```

## 🏛️ Architecture

### Compilation Pipeline

```
.ar source → Lexer → Parser → Type Inference → Two-Pass Codegen → LLVM IR → Native Binary
```

### Two-Pass Compilation

1. **Declaration Pass**: Register all function signatures, types, and externs
2. **Definition Pass**: Generate function bodies, resolve forward references

This enables:

- Functions calling each other in any order
- Modular standard library design
- Self-hosting capabilities

## 🚀 Performance

- **Native Code**: Direct LLVM compilation, no VM or interpreter
- **Zero-Cost Abstractions**: High-level UI code compiles to efficient machine code
- **Hardware Acceleration**: CoreAI uses Apple Accelerate for matrix operations
- **Minimal Runtime**: Lightweight Objective-C bridge to macOS frameworks

## 🔧 Development

### Adding New Frameworks

1. Create `stdlib/MyFramework.ar`
2. Add native bridges in `lib/runtime.m` if needed
3. Update `lib/codegen.ml` for new intrinsics
4. Write tests in `tests/test_myframework.ar`

### Compiler Development

```bash
# Make changes to lib/
vim lib/codegen.ml

# Rebuild
dune build

# Test changes
dune exec -- ardium run test.ar
```

## 📊 Version History

- **2.0.0 "Ascension"** (2026-01-02)
  - 8-framework ecosystem
  - Two-pass compilation
  - __builtin_ function system
  - stdlib/ import path
  - Self-hosting foundation

- **1.x** - Initial release

## 🤝 Contributing

Contributions welcome! Areas of focus:

- Performance optimization
- Additional framework functionality
- More comprehensive testing
- Documentation improvements
- Example programs

## ⚖️ License

MIT License

## 🙏 Acknowledgments

- LLVM Project
- Apple Developer Tools
- OCaml Community

---

**Built with ❤️ for modern macOS development**
