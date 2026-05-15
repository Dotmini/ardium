# Ardium 2.0.0 - Complete Documentation

## Table of Contents

1. [Getting Started](#getting-started)
2. [Core Framework](#core-framework)
3. [CoreUI Framework](#coreui-framework)
4. [CoreAI Framework](#coreai-framework)
5. [CoreData Framework](#coredata-framework)
6. [CoreNetwork Framework](#corenetwork-framework)
7. [CoreCrypto Framework](#corecrypto-framework)
8. [CoreKits Framework](#corekits-framework)
9. [PlaygroundSupport Framework](#playgroundsupport-framework)
10. [Architecture](#architecture)

---

## Getting Started

### Installation

```bash
# Clone the repository
cd /Users/dotmini/Documents/ardium

# Build the compiler
dune build

# Run your first program
dune exec -- ardium run myfile.ar
```

### Hello World

```ardium
import "Core"

fn main() {
    println("Hello, Ardium 2.0!")
    return 0
}
```

### Compile and Run

```bash
# Run directly
dune exec -- ardium run hello.ar

# Compile to executable
dune exec -- ardium build hello.ar -o hello
./hello
```

---

## Core Framework

**Import**: `import "Core"`

The Core framework provides fundamental I/O and utility functions.

### Functions

#### `print(message: string)`

Prints text to console without newline.

```ardium
print("Hello")
print(" World")  // Output: Hello World
```

#### `println(message: string)`

Prints text to console with newline.

```ardium
println("Line 1")
println("Line 2")
// Output:
// Line 1
// Line 2
```

### Example

```ardium
import "Core"

fn main() {
    print("User: ")
    println("John")
    println("Status: Active")
    return 0
}
```

---

## CoreUI Framework

**Import**: `import "CoreUI"`

SwiftUI-inspired declarative UI framework for macOS applications.

### Components

#### `App(title: string, content_fn: function)`

Creates an application window and runs the GUI event loop.

```ardium
App("My App", resolve_symbol("build_ui"))
```

#### `VStack(content_fn: function)`

Vertical stack layout - arranges children top to bottom.

```ardium
VStack(resolve_symbol("my_vertical_content"))
```

#### `HStack(content_fn: function)`

Horizontal stack layout - arranges children left to right.

```ardium
HStack(resolve_symbol("my_horizontal_content"))
```

#### `ZStack(content_fn: function)`

Z-axis stack - overlays children on top of each other.

```ardium
ZStack(resolve_symbol("overlay_content"))
```

#### `Title(text: string)`

Large bold title text (24pt).

```ardium
Title("Welcome")
```

#### `Subtitle(text: string)`

Medium subtitle text (18pt).

```ardium
Subtitle("Please login")
```

#### `Text(text: string)`

Regular body text (14pt).

```ardium
Text("Username:")
```

#### `Button(label: string, callback: function)`

Interactive button that calls a function when clicked.

```ardium
fn on_click() {
    println("Button clicked!")
}

// In your UI:
Button("Click Me", resolve_symbol("on_click"))
```

#### `TextField(placeholder: string) -> handle`

Single-line text input field. Returns a handle for value retrieval.

```ardium
let field = TextField("Enter name")
let value = get_input_value(field)
```

#### `SecureField(placeholder: string) -> handle`

Password input field (shows dots instead of characters).

```ardium
let password_field = SecureField("Password")
```

#### `Image(path: string)`

Displays an image from file path.

```ardium
Image("/path/to/logo.png")
```

#### `Spacer(size: int)`

Adds vertical or horizontal spacing.

```ardium
Spacer(20)  // 20px gap
```

#### `Padding(amount: int)`

Adds padding around content.

```ardium
Padding(10)
```

### Complete Example

```ardium
import "Core"
import "CoreUI"

let username_field = 0

fn handle_login() {
    println("Login clicked!")
    let handle = peek(resolve_symbol("username_field"))
    let name = get_input_value(handle)
    println("User: " + name)
}

fn build_ui() {
    VStack(resolve_symbol("main_content"))
}

fn main_content() {
    Title("SPU AI CLUB")
    Subtitle("Ardium 2.0")
    Spacer(20)
    
    Text("Username:")
    let field = TextField("Enter ID")
    poke(resolve_symbol("username_field"), field)
    
    Spacer(10)
    Button("Login", resolve_symbol("handle_login"))
}

fn main() {
    App("Login Demo", resolve_symbol("build_ui"))
    return 0
}
```

---

## CoreAI Framework

**Import**: `import "CoreAI"`

High-performance AI/ML primitives with hardware acceleration.

### Functions

#### `MatMul(A: array, B: array, C: array, N: int)`

Matrix multiplication using Apple Accelerate framework (BLAS).

```ardium
// Multiply two NxN matrices
MatMul(matrix_a, matrix_b, result, 100)
```

#### `Tensor(shape: array) -> handle`

Creates a tensor data structure (stub for future ML operations).

```ardium
let tensor = Tensor(shape)
```

---

## CoreData Framework

**Import**: `import "CoreData"`

File I/O and data persistence.

### Functions

#### `Save(path: string, content: string)`

Writes text content to a file.

```ardium
Save("/tmp/data.txt", "Hello World")
```

#### `Load(path: string) -> string`

Reads text content from a file.

```ardium
let content = Load("/tmp/data.txt")
println(content)  // "Hello World"
```

#### `IsExists(path: string) -> int`

Checks if file exists (stub).

```ardium
let exists = IsExists("/tmp/data.txt")
```

### Example

```ardium
import "Core"
import "CoreData"

fn main() {
    // Save data
    Save("/tmp/user_data.txt", "John Doe\\nAge: 25")
    
    // Load data
    let data = Load("/tmp/user_data.txt")
    println("Loaded:")
    println(data)
    
    return 0
}
```

---

## CoreNetwork Framework

**Import**: `import "CoreNetwork"`

Modern networking with HTTP support.

### Functions

#### `Fetch(url: string) -> string`

Performs HTTP GET request and returns response body.

```ardium
let html = Fetch("https://example.com")
println(html)
```

#### `Post(url: string, data: string) -> string`

HTTP POST request (stub).

```ardium
let response = Post("https://api.example.com", "payload")
```

### Example

```ardium
import "Core"
import "CoreNetwork"

fn main() {
    println("Fetching data...")
    let response = Fetch("https://httpbin.org/get")
    println(response)
    return 0
}
```

---

## CoreCrypto Framework

**Import**: `import "CoreCrypto"`

Cryptographic primitives for secure computing.

### Functions

#### `SHA256(input: string) -> string`

Computes SHA-256 hash (simplified stub).

```ardium
let hash = SHA256("password123")
println(hash)  // Returns hash representation
```

#### `MD5(input: string) -> string`

Computes MD5 hash (stub).

```ardium
let hash = MD5("data")
```

### Example

```ardium
import "Core"
import "CoreCrypto"

fn main() {
    let password = "mysecret"
    let hash = SHA256(password)
    
    println("Password hash:")
    println(hash)
    
    return 0
}
```

---

## CoreKits Framework

**Import**: `import "CoreKits"`

General-purpose utilities and data structures.

### Functions

#### `List() -> handle`

Creates a dynamic list structure.

```ardium
let my_list = List()
```

#### `Log(message: string)`

Debug logging with prefix.

```ardium
Log("Debug: Processing item")
// Output: [LOG] Debug: Processing item
```

### Example

```ardium
import "Core"
import "CoreKits"

fn main() {
    Log("Application started")
    let data = List()
    Log("Data structure initialized")
    return 0
}
```

---

## PlaygroundSupport Framework

**Import**: `import "PlaygroundSupport"`

Live development and debugging tools.

### Functions

#### `Live(view_fn: function)`

Enables hot-reloading for UI development (stub).

```ardium
Live(resolve_symbol("my_view"))
```

#### `DebugUI()`

Shows UI inspector overlay (stub).

```ardium
DebugUI()
```

### Example

```ardium
import "Core"
import "PlaygroundSupport"

fn my_experimental_view() {
    println("Experimental UI")
}

fn main() {
    Live(resolve_symbol("my_experimental_view"))
    DebugUI()
    return 0
}
```

---

## Architecture

### Compiler Pipeline

```
Source (.ar) → Lexer → Parser → Type Inference → Codegen → LLVM IR → Native Binary
```

### Two-Pass Compilation

**Pass 1 - Declaration Phase**:

- Declares all function signatures
- Registers type definitions
- Creates extern declarations

**Pass 2 - Definition Phase**:

- Generates function bodies
- Resolves forward references
- Emits final LLVM IR

### Runtime Architecture

```
┌─────────────────────────────────────┐
│     Ardium Standard Library         │
│  (Core, CoreUI, CoreAI, etc.)       │
└──────────────┬──────────────────────┘
               │ __builtin_* calls
┌──────────────▼──────────────────────┐
│      Compiler Intrinsics             │
│  (__builtin_print, peek, poke, etc.) │
└──────────────┬──────────────────────┘
               │ LLVM IR
┌──────────────▼──────────────────────┐
│      Native Runtime (runtime.m)      │
│  Obj-C GUI, File I/O, Networking     │
└──────────────┬──────────────────────┘
               │
┌──────────────▼──────────────────────┐
│   macOS Frameworks (Cocoa, etc.)     │
└──────────────────────────────────────┘
```

### Memory Model

- **Stack**: Local variables, function parameters
- **Heap**: `malloc()` allocations, string literals
- **Globals**: `let` declarations at top level

### Build System

```bash
# Development
dune build          # Compile compiler
dune exec ardium    # Run compiler

# Distribution
dune build --release
dune install
```

---

## Advanced Topics

### Function References

Use `resolve_symbol()` to get function pointers:

```ardium
fn callback() {
    println("Called!")
}

let fn_ptr = resolve_symbol("callback")
// Pass fn_ptr to other functions
```

### Memory Management

```ardium
let ptr = malloc(1024)    // Allocate
poke(ptr, 42)             // Write
let val = peek(ptr)       // Read
// No explicit free yet - managed by arena
```

### String Concatenation

```ardium
let name = "John"
let greeting = "Hello, " + name + "!"
println(greeting)
```

---

## Troubleshooting

### Common Issues

**Import not found**:

- Ensure `.ar` file exists in `stdlib/` directory
- Check spelling and case sensitivity

**Function undefined**:

- Verify framework is imported
- Check function name spelling

**Compilation errors**:

```bash
# Rebuild from scratch
dune clean
dune build
```

---

## Contributing

Ardium 2.0.0 is designed for extensibility. To add new frameworks:

1. Create `stdlib/YourFramework.ar`
2. Add native bridges in `lib/runtime.m`
3. Update `lib/codegen.ml` if adding intrinsics
4. Write tests in `tests/`

---

**Version**: 2.0.0  
**License**: MIT  
**Repository**: /Users/dotmini/Documents/ardium
