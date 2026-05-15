# 📘 The Ardium Manual (v2.3.0)

Welcome to the definitive guide for **Ardium**, the high-performance systems programming language designed for modern macOS development.

## 🌟 Philosophy

Ardium is built on three core pillars:

1. **Native Performance**: Compiles directly to machine code via LLVM. No VM, no Interpreter.
2. **Safety & Freedom**: Offers strict type safety and RAII memory management, but allows raw pointer access when needed.
3. **Modern UI First**: The only systems language with a first-class, declarative UI framework (`CoreUI`) inspired by SwiftUI.

---

## 🛠️ Installation & Toolchain

### Installing via Package (Recommended)

Download the `.pkg` installer for your version:

- **Location**: `/usr/local/ardium/`
- **Binaries added to PATH**: `arc`, `ardium`

### Building from Source

```bash
git clone https://github.com/dotmini/ardium.git
cd ardium
dune build
```

### The `arc` CLI

The `arc` tool (Ardium Compiler) is your primary interface.

| Command | Usage | Description |
|---------|-------|-------------|
| `build` | `arc build <file.ar> -o <out>` | Compiles source to native executable with `-O3` optimizations. |
| `run` | `arc run <file.ar>` | JIT compiles and runs the file immediately. Great for dev. |
| `dev` | `arc dev <file.ar>` | Watch mode. Recompiles on file save (Hot Reload). |
| `test` | `arc test <test_dir>` | Runs all `.ar` files in the directory as test cases. |
| `new` | `arc new <project_name>` | Scaffolds a new project with `ardium.toml`. |
| `header`| `arc header <file.ar>` | Generates C headers (`.h`) for `@export` functions. |
| `lsp` | `arc lsp` | Starts the Language Server for VS Code. |

---

## 📖 Language Reference

### 1. Variables & Types

Ardium uses type inference. You rarely need to specify types.

**Immutable Bindings (Default)**

```rust
let name = "Ardium"; // String
let version = 2.3;   // Float (Double)
let is_beta = true;  // Bool
```

**Mutable Variables**
Use `var` (or `let mut` in older versions) for values that change.

```rust
var count = 0;
count = count + 1;
```

**Explicit Types**

```rust
let age: int = 25;
let pi: float = 3.14159;
let data: i8_ptr = alloc(64);
```

### 2. Control Flow

**If / Else**
Braces `{}` are mandatory. Parentheses `()` for conditions are optional but recommended.

```rust
if (score > 90) {
    println("A");
} elif (score > 80) {
    println("B");
} else {
    println("C");
}
```

**Loops**
Standard `while` loop.

```rust
var i = 0;
while (i < 10) {
    print(i);
    i = i + 1;
}
```

**Loop Control**

- `break`: Exit loop immediately.
- `continue`: Skip to next iteration.

### 3. Functions

Functions are declared with `fn`. They return the value of the last expression or `return` statement.

```rust
fn add(a, b) {
    return a + b; // Explicit return
}

fn multiply(x, y) {
    x * y // Implicit return supported in blocks
}
```

### 4. Memory Management (RAII) 🛡️ **NEW in v2.3**

Ardium v2.3 introduces **Scope-Based Memory Management**.

**Automatic Cleanup (`@owned`)**
Mark a variable as `@owned` to tell the compiler to free it automatically when it goes out of scope (end of block, function, or loop).

```rust
fn process_image() {
    @owned let buffer = alloc(1024 * 1024); // 1MB buffer
    
    // ... work with buffer ...
    
} // <--- 'free(buffer)' is automatically inserted here!
```

**Manual Allocator**
For advanced control, use `alloc` (malloc) and `free`.

```rust
let ptr = alloc(128); // Standard heap allocation
poke(ptr, 0xFF);      // Write byte
let val = peek(ptr);  // Read byte
free(ptr);            // Manual release
```

### 5. Interoperability (FFI)

**Calling C Functions**
Use `extern fn` to bind standard C libraries.

```rust
@External("libc")
extern fn printf(fmt, ...);

fn main() {
    printf("Number: %d\n", 42);
    return 0;
}
```

**Exporting to C**
Use `@export` to make an Ardium function available to C linkers.

```rust
@export
fn my_api(x) {
    return x * 2;
}
```

---

## 🎨 GUI Framework (`CoreUI`)

Ardium's crown jewel is its native bridge to Cocoa/AppKit. Write declarative UI that compiles to native macOS calls.

### Setup

```rust
import "Core"

fn main() {
    init_apple_gui(); // Initialize NSApp
    
    // ... UI Code ...
    
    run_apple_gui(); // Start Event Loop (Blocking)
    return 0;
}
```

### Layout System

Ardium uses a stack-based layout system similar to SwiftUI.

**VStack (Vertical)**
Arranges elements from top to bottom.

```rust
@VClass
fn MainLayout() {
    text("Title", 32, 1);
    text("Description", 16, 0);
}
```

**HStack (Horizontal)**
Arranges elements from left to right.

```rust
@HClass
fn Toolbar() {
    Button("Save", on_save);
    Button("Cancel", on_cancel);
}
```

### Widgets

**Text**

```rust
text("Hello", 24, 1); // Content, Size, Bold (0/1)
```

**Button**

```rust
fn on_click() {
    println("Clicked!");
}

Button("Click Me", on_click);
```

**Image** (Asset Catalog or Path)

```rust
Image("logo.png", 100, 100);
```

**TextField**

```rust
TextField("Placeholder", on_text_change);
```

### Window Management

```rust
// Title, X, Y, Width, Height
create_apple_window("Ardium App", 0, 0, 800, 600);
set_modern_background(); // Applies "Midnight Charcoal" theme
```

---

## 📚 Standard Library Overview

| Library | Functionality |
|---------|---------------|
| `Core` | Basic IO (`print`, `println`), String conversions. |
| `CoreUI` | GUI widgets and windowing bindings. |
| `CoreAI` | Matrix math, Tensor ops (via Apple Accelerate). |
| `CoreNetwork` | HTTP client (`Fetch`). |
| `CoreData` | File system (`Load`, `Save`). |
| `CoreCrypto` | Hashing (`SHA256`) and security. |

---

## 🧩 Advanced Topics

### Alloca Hoisting (Stack Optimization)

The compiler automatically moves stack allocations (`alloca`) from loops to the function entry block.

- **Benefit**: Prevents stack overflow in tight loops (e.g., `while (true) { let x = ... }`).
- **Behavior**: You don't need to do anything; it's automatic.

### Global State handler

Use decorators to handle global events or state.

```rust
@GLOBAL(state)
fn my_state_handler() {
    // Code runs when global state changes
}
```

---

*(C) 2026 Arsenal Engine Project*
