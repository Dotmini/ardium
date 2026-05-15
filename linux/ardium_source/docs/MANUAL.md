# 📘 Ardium Manual: The Hacker-Dev's Bible

Welcome to the comprehensive documentation for **Ardium**, the systems language designed for high-performance development and low-level exploration.

---

## 📚 Table of Contents

1. [Installation & Setup](#1-installation--setup)
2. [Quick Start](#2-quick-start)
3. [The CLI Toolchain](#3-the-cli-toolchain)
4. [Language Guide](#4-language-guide)
   - [Core Syntax](#core-syntax)
   - [Memory Model](#memory-model)
   - [Hybrid Interop](#hybrid-interop)
5. [Systems Programming (Kernel Mode)](#5-systems-programming-kernel-mode)
6. [Architecture Deep Dive](#6-architecture-deep-dive)

---

## 1. Installation & Setup

### Option A: Standalone Binary (Recommended)

Ardium is distributed as a single, self-contained executable. It does **not** requires OCaml, LLVM, or any external package managers on your machine.

1. **Download the Installer**:
   Get the latest `Ardium.dmg` or `Ardium.pkg` from the release page.

2. **Run the Installer**:
   - **DMG**: Open `Ardium.dmg` and run the `Ardium.pkg` inside.
   - **PKG**: Double-click `Ardium.pkg` to install.
   - This places `ar` and `ar-up` in `/usr/local/bin` and the library in `/usr/local/lib/ardium`.

3. **Verify**:

   ```bash
   ar --version
   ```

### Option B: Building from Source

If you are contributing to the compiler itself:

1. Install Dependecies: `brew install llvm ocaml opam dune`
2. Clone repo & build:

   ```bash
   make build
   ```

---

## 2. Quick Start

Create a new file `hello.ar`:

```rust
extern fn printf(fmt: string, ...): i32;

fn main() {
    printf("Hello, Ardium from the Kernel!\n");
}
```

Run it immediately:

```bash
ar run hello.ar
```

---

## 3. The CLI Toolchain

The `ar` command is your swiss-army knife.

| Command | Description | Example |
| :--- | :--- | :--- |
| `ar run` | Compile & Execute one-shot | `ar run main.ar` |
| `ar build` | Compile to optimal binary | `ar build main.ar -o app` |
| `ar dev` | Watch mode (Hot Reload) | `ar dev main.ar` |
| `ar test` | Run integration tests | `ar test tests/*.ar` |
| `ar install`| Install dependencies (`ardium.json`) | `ar install` |
| `ar lsp` | Start Language Server | (Used by IDE plugins) |

**Flags**:

- `--lib`: Build as a dynamic library (`.dylib`).
- `--no-std`: Disable standard library (for Kernels).
- `--info`: Dump symbol exports as JSON.

---

## 4. Language Guide

### Core Syntax

Ardium syntax is Rust-like but simplified for hacking.

```rust
// Functions
fn add(a: i32, b: i32): i32 {
    return a + b;
}

// Variables
let x = 10;
let mut y = 20;
y = 30;

// Modules
import "math.ar"; // Imports local file
import Foundation; // Imports Framework
```

### Hybrid Interop

Ardium bridges seamlessly with C and Swift.

1. **Calling C**: Just declare `extern`.

   ```rust
   extern fn malloc(size: i64): i64;
   ```

2. **Exporting to Swift**: Use `@export`.

   ```rust
   @export
   fn swift_can_call_this() { ... }
   ```

   `ar build --lib` will automatically generate the C header and Swift Module Map.

### Reflective Resolution

Resolve symbols dynamically at runtime without linking:

```rust
let ptr = resolve_symbol("libSystem.dylib", "printf");
// Use 'ptr' as a function pointer
```

---

## 5. Systems Programming (Kernel Mode)

Ardium honors its roots as a tool for OS dev.

### Freestanding Mode

Use `--no-std` to prevent linking against `libSystem` or `libc`.

```bash
ar build kernel.ar --no-std
```

### Interrupts

Write literal Interrupt Service Routines (ISRs) with the requested calling convention (`x86_intr`):

```rust
@interrupt
fn keyboard_handler() {
   // ...
}
```

### Direct I/O

Talk to hardware ports directly (inline assembly `inb`/`outb`):

```rust
fn driver() {
    let status = ioport_in(0x64);
    ioport_out(0x60, 0xFF);
}
```

---

## 6. Architecture Deep Dive

For those who want to hack the compiler.

### Pipeline

1. **Lexer/Parser** (`lib/lexer.mll`, `lib/parser.mly`): OCamllex/Menhir based.
2. **AST** (`lib/ast.ml`): Simple typed tree.
3. **Codegen** (`lib/codegen.ml`): Generates LLVM IR.
   - **AI JIT**: `lib/ai_jit.ml` allows injecting assembly patterns during codegen.
   - **Phantom Runtime**: `lib/runtime.c` is embedded as a string.

### Self-Healing Logic

The compiler embeds its own runtime source code into the binary.

- On launch, if `runtime.c` library is missing, `ar` extracts the C code to `/tmp` and compiles it on the fly using `clang`.
- This ensures the compiler *cannnot be broken* by missing files.

### Memory Layout

Ardium uses an **Arena Allocator** model (implied). There is no Garbage Collector. You allocation memory, and it is freed when the arena resets (or process ends). This is ideal for command-line tools and request handlers.
