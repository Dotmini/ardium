# Arc Programming Language (Arc Language)

**Version 3.0.0 (Titan Engine)**
**Architect & Founder:** Tirawat Nantamas (Dotmini Software / SPU AI CLUB)

Arc is an advanced, high-performance systems and application programming language engineered for Apple Silicon (macOS) and modern platforms. It integrates a declarative component-driven UI paradigm, hardware acceleration with Metal & Apple Accelerate, high-performance JIT/AOT compilation, and native C/Swift interoperability.

## 1. Overview and Key Capabilities

- **ARC CLI (`arc`)**: Unified, modern command-line interface for running, building, testing, and managing Arc projects.
- **Titan Engine & JIT**: High-speed native runtime execution engine with zero-copy buffer sharing and micro-kernel architecture.
- **Declarative User Interface**: Structural UI layout system (`VStack`, `HStack`, `ZStack`, `Button`, `TextField`) with native Metal & Cocoa acceleration.
- **Hardware-Accelerated AI & Computation**: Direct access to Apple Silicon Neural Engine (CoreML/Vision) and GPU pipelines via Metal shaders.
- **Native Networking & Systems Stack**: High-throughput networking, POSIX threading, and robust I/O.
- **Zero-Cost Abstractions**: Compile to optimized native machine code with `-O3` performance.

## 2. Quick Start

### 2.1. System Requirements

- macOS 12.0+ (Optimized for Apple Silicon M1/M2/M3/M4) or Linux
- Clang / Xcode Command Line Tools (`clang++` with C++20 support)
- Apple Metal Toolchain (for graphics & vision shaders)

### 2.2. Building the Arc Language Toolchain

To build the Arc compiler, CLI (`arc`), and Titan runtime:

```bash
# Build Arc CLI and Titan Engine
make build

# Or use the build script directly
./build_cli.sh
```

### 2.3. CLI Commands

```bash
# Run an Arc source file directly (JIT)
./arc run hello.ar

# Compile an Arc source file to a native standalone binary
./arc build hello.ar -o myapp
./myapp

# Create a new Arc Language project
./arc new MyAwesomeProject

# Watch mode with Hot-Reloading
./arc dev src/main.ar

# Verify system toolchain & health check
./arc doctor

# Run automated test suite
./arc test
```

import "Core"
import "CoreUI"

fn on_click() {
    println("Event registered: Button click.")
}

fn build_ui() {
    VStack(resolve_symbol("content"))
}

fn content() {
    Title("Ardium GUI Framework")
    Subtitle("Version 2.0 Ascension")
    Spacer(20)
    Button("Execute Action", resolve_symbol("on_click"))
}

fn main() {
    App("Application Instance", resolve_symbol("build_ui"))
    return 0
}
```

## 4. Documentation References

Comprehensive documentation for the Ardium programming language and its standard library can be found in the following directories:

- **[API Reference](docs/API_REFERENCE.md)**: Detailed specifications of all available framework functions and modules.
- **[Implementation Walkthrough](../../.gemini/antigravity/brain/11a28a47-60fd-4dcf-a562-0f893f8592b9/walkthrough.md)**: Architectural and implementation details.

## 5. Standard Library Ecosystem

The Ardium 2.0.0 release encompasses eight foundational frameworks:

| Framework | Primary Objective | Key Components |
|-----------|-------------------|----------------|
| **Core** | System Fundamentals | Standard I/O operations (`print`, `println`) |
| **CoreUI** | Interface Rendering | `VStack`, `HStack`, `Button`, `TextField`, `Image` |
| **CoreAI** | Machine Learning | Hardware-accelerated matrix and tensor mathematics |
| **CoreData** | Persistence Management | File descriptors, input/output streams (`Load`, `Save`) |
| **CoreNetwork** | Network Protocol Stack| HTTP client utilities (`Fetch`) |
| **CoreCrypto** | Security and Integrity | Cryptographic hashing (`SHA256`) |
| **CoreKits** | Utility Functions | Data structures, diagnostic logging |
| **PlaygroundSupport**| Interactive Development | Hot-module reloading, debugging utilities |

## 6. Toolchain and Build System

The Ardium compiler provides a robust command-line interface for development and compilation:

```bash
# Purge build artifacts and recompile
dune clean && dune build

# Interpret and execute an Ardium source file
dune exec -- ardium run application.ar

# Compile an Ardium source file to a native executable
dune exec -- ardium build application.ar -o application_binary
./application_binary

# Execute the automated test suite
dune exec -- ardium run tests/test_frameworks_basic.ar
```

## 7. Project Architecture

### 7.1. Directory Structure

```text
ardium/
├── bin/              # Compiler executable entry point
│   └── main.ml
├── lib/              # Core compiler implementation
│   ├── lexer.mll     # Lexical analysis logic
│   ├── parser.mly    # Syntactic analysis logic
│   ├── codegen.ml    # LLVM Intermediate Representation generation
│   └── runtime.m     # Native Objective-C runtime bridge
├── stdlib/           # Standard library frameworks
│   ├── Core.ar
│   ├── CoreUI.ar
│   └── ...
├── tests/            # Automated verification suite
└── docs/             # Technical documentation
```

### 7.2. Compilation Pipeline

The Ardium compiler translates source code into native executables through a highly optimized pipeline:

1. **Lexical Analysis**: Source tokenization via `ocamllex`.
2. **Syntactic Analysis**: Abstract Syntax Tree (AST) construction via `menhir`.
3. **Semantic Analysis**: Type inference and validation.
4. **Intermediate Representation**: Two-pass LLVM IR code generation.
5. **Native Code Generation**: Final compilation to machine code utilizing the LLVM backend.

The **Two-Pass Compilation** model operates as follows:
- **Pass 1 (Declaration)**: Registration of global signatures, types, and external symbol definitions.
- **Pass 2 (Definition)**: Synthesis of function bodies and resolution of forward-referenced identifiers.

## 8. Versioning History

- **Version 2.0.0 (Ascension)**
  - Introduction of the eight-framework standard library ecosystem.
  - Implementation of the two-pass compilation architecture.
  - Integration of the `__builtin_` intrinsics system.
  - Structural refactoring of the `stdlib/` import namespace.
  - Establishment of self-hosting compiler foundations.

- **Version 1.x**
  - Initial conceptual release and proof-of-concept.

## 9. Contribution Guidelines

We welcome contributions from the community. Primary areas of interest for future development include:

- Compiler optimization passes and LLVM backend tuning.
- Expansion of the standard library frameworks.
- Enhancements to the automated test suite coverage.
- Technical documentation refinement.

## 10. License

The Ardium programming language and its associated toolchain are distributed under the MIT License.

## 11. Acknowledgments

The development of Ardium is made possible through the utilization of:

- The LLVM Compiler Infrastructure Project
- Apple Developer Tools and Frameworks
- The OCaml Programming Language and Community
