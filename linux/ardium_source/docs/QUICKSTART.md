# Ardium 2.0.0 - Quick Start Guide

## Installation (5 minutes)

```bash
cd /Users/dotmini/Documents/ardium
dune build
```

## Your First Program (2 minutes)

Create `hello.ar`:

```ardium
import "Core"

fn main() {
    println("Hello, Ardium! 🚀")
    return 0
}
```

Run it:

```bash
dune exec -- ardium run hello.ar
```

## Build a GUI App (10 minutes)

Create `gui_demo.ar`:

```ardium
import "Core"
import "CoreUI"

fn on_button_click() {
    println("🎉 Button was clicked!")
}

fn my_ui() {
    VStack(resolve_symbol("my_content"))
}

fn my_content() {
    Title("My First GUI")
    Spacer(20)
    Button("Click Me!", resolve_symbol("on_button_click"))
}

fn main() {
    App("Demo App", resolve_symbol("my_ui"))
    return 0
}
```

Run it:

```bash
dune exec -- ardium run gui_demo.ar
```

You'll see a macOS window with a button!

## Try File I/O (3 minutes)

Create `file_demo.ar`:

```ardium
import "Core"
import "CoreData"

fn main() {
    println("Writing file...")
    Save("/tmp/test.txt", "Ardium 2.0 rocks!")
    
    println("Reading file...")
    let content = Load("/tmp/test.txt")
    println(content)
    
    return 0
}
```

## Next Steps

- Read the [API Reference](API_REFERENCE.md)
- Check out examples in `tests/`
- Build something amazing!

## Common Commands

```bash
# Build compiler
dune build

# Run a program
dune exec -- ardium run myapp.ar

# Compile to executable
dune exec -- ardium build myapp.ar -o myapp
./myapp

# Clean build
dune clean
```

## Need Help?

Check the full documentation in `docs/API_REFERENCE.md`
