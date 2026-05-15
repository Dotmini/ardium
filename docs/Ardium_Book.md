# 📘 The Ardium Book: Zero to Hero

**Version**: 2.5.0  
**Author**: Dotmini Software & SPU AI Club

---

## 🚀 Chapter 1: Welcome to Ardium

Welcome to the future of high-performance computing.

**Ardium** is a compiled programming language designed for one purpose: **Maximum Performance with Modern Syntax.**

- **No Garbage Collector (GC)**: Unlike Java, Python, or Go, Ardium doesn't pause your app to clean up memory.
- **Native compilation**: Your code turns into machine language (Binary) that the CPU understands directly.
- **Modern Feel**: It looks like Python or Swift, but runs as fast as C.

---

## 🏁 Chapter 2: Getting Started

### 2.1 Installation

You likely have the installer `Ardium_v2.5.0.pkg`. Double-click it to install.
To verify, open your terminal and type:

```bash
arc --version
# Output: Ardium Compiler v2.5.0
```

### 2.2 Your First Project

Let's create a project named `hello`.

```bash
arc new hello
cd hello
```

Inside, you'll find `src/main.ar`.

### 2.3 Running Codde

Run your code instantly with the JIT (Just-In-Time) engine:

```bash
arc run src/main.ar
```

Or build a standalone executable:

```bash
arc build src/main.ar
./a.out
```

---

## 🎓 Chapter 3: Ardium Basics (Zero Basic)

If you've never coded, start here.

### 3.1 Variables

Think of variables as boxes that hold data.

- `let`: A **sealed** box. You put data in once, and you can't change it.
- `var`: An **open** box. You can change what's inside.

```rust
fn main() {
    let name = "Dotmini";  // Immutable (Cannot change)
    var age = 20;          // Mutable (Can change)
    
    age = 21;              // OK!
    // name = "Other";     // Error! 'name' is let
    
    println("Hello, " + name);
}
```

### 3.2 Functions

Functions are distinct jobs or tasks.

```rust
// 'fn' keyword starts a function
fn add(a, b) {
    return a + b;
}

fn main() {
    let result = add(10, 5);
    println(result); // Prints 15
}
```

### 3.3 Logic (If/Else)

Making decisions.

```rust
let score = 80;

if (score >= 50) {
    println("Passed!");
} else {
    println("Try again.");
}
```

### 3.4 Loops

Repeating things.

```rust
var i = 0;
while (i < 5) {
    println(i);
    i = i + 1;
}
```

---

## 🧠 Chapter 4: The Memory Model (Advanced)

This is Ardium's superpower. It manages memory without a "Garbage Collector" (which slows things down) but is safer than C.

### 4.1 Stack vs Heap

- **Stack**: Fast, temporary memory for simple numbers and small vars.
- **Heap**: Large memory for big data (images, massive text).

### 4.2 Ownership (`@owned`)

When you allocate memory, you "own" it. Ardium helps you clean it up automatically when you're done.

```rust
fn process_image() {
    // @owned tells Ardium: "Free this memory when this function ends"
    @owned var buffer = malloc(1024); 
    
    // Do work...
    poke(buffer, 255);
    
} // <--- Ardium automatically calls free(buffer) here!
```

**Why do we care?**
In C, if you forget to free, your RAM fills up (Memory Leak).
In Python, the GC cleans it, but freezes your app.
In **Ardium**, it's automatic AND fast.

### 4.3 Reference Counting (ARC)

For things shared like strings, Ardium counts how many people are holding it. When the count hits 0, it deletes itself.

---

## 🎨 Chapter 5: CoreUI (Native Apps)

Build real macOS apps, not web apps in a wrapper.

### 5.1 The Setup

```rust
import "Core"

fn main() {
    init_apple_gui();
    create_apple_window("My App", 100, 100, 400, 300);
    render_ui();
    run_apple_gui();
}
```

### 5.2 Layouts (`@VClass`, `@HClass`)

- `@VClass`: Stacks items Vertically (Top to Bottom).
- `@HClass`: Stacks items Horizontally (Left to Right).

```rust
@VClass
fn render_ui() {
    text("Login", 24, 1);
    
    @HClass {
        text("Username: ", 14, 0);
        // TextField would go here
    }
    
    button("Login", on_login_click);
}

fn on_login_click() {
    println("Logged in!");
}
```

---

## 📚 Chapter 6: Cheat Sheet

### Commands

| Command | Description |
|---|---|
| `arc new <name>` | Create project |
| `arc run <file>` | Run instantly (Fast) |
| `arc build <file>` | Build optimized binary |

### Standard Library

| Function | Description |
|---|---|
| `println(x)` | Print value with new line |
| `malloc(size)` | Allocate memory (Bytes) |
| `free(ptr)` | Free memory manually |
| `sleep(sec)` | Pause program |
b