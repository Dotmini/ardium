# Ardium Apple GUI Framework Guide

Ardium provides a powerful bridge to macOS native frameworks via the **Core Standard Library** (`import Core`).

## 1. Quick Start

Using `import Core`, you get access to succinct GUI functions without manual `extern` declarations.

```ardium
import Core

fn main() {
    println("🚀 Launching Ardium GUI...");
    
    // 1. Initialize System
    agui();
    
    // 2. Create Window
    viewer("Ardium App", 100, 100, 800, 600);
    
    // 3. Set Background
    background();
    
    // 4. Run Loop (Blocking)
    run_apple_gui();
    return 0;
}
```

## 2. Layout System (Declarative UI)

Arrange elements using decorators (`@VClass`, `@HClass`) and `text()` without calculating coordinates.

### Decorators

- **`@VClass`**: Vertical Stack (Top-to-Bottom)
- **`@HClass`**: Horizontal Stack (Left-to-Right)

### APIs

- **`text(msg, size, bold)`**: Adds text using the current layout cursor.
- **`Left` / `Center` / `Right`**: Constants for alignment (0, 1, 2).

### Example

```ardium
import Core

@VClass
fn Header() {
    text("My Super App", 40, 1);
    text("Version 2.0", 20, 0);
}

@HClass
fn StatusBar() {
    text("Status: Online", 14, 1);
    text("System: Stable", 14, 0);
}

fn main() {
    agui();
    viewer("Layout App", 100, 100, 800, 600);
    background();
    
    Header();
    StatusBar();
    
    run_apple_gui();
    return 0;
}
```

## 3. Core API Reference

All functions are available automatically when you `import Core`.

### `agui()`

Initializes the `NSApplication`. Call this first.

### `viewer(title, x, y, w, h)`

Creates a native macOS window.

- `title`: String
- `x, y`: Position (from bottom-left)
- `w, h`: Size

### `background()`

Sets the window to the modern "Midnight Charcoal" theme.

### `text(msg, size, bold)`

Adds auto-layout text. Movement depends on the active `@VClass` or `@HClass`.

### `run_apple_gui()`

Starts the main event loop. This blocks until the app closes.

## 4. Manual / Advanced API

For fine-grained control, you can still use the raw functions (automatically aliased by Core):

- `add_styled_text(text, x, y, w, h, size, bold, align)`: Manual positioning.
- `init_apple_gui()`: Same as `agui()`.
- `create_apple_window(...)`: Same as `viewer(...)`.
- `set_modern_background()`: Same as `background()`.

---
*(C) 2026 Arsenal Engine Project - Ardium Ascension*
