# FFI & External Integration

Ardium makes it easy to call functions from external C/C++ or Rust libraries using the `@External` decorator.

## Using `@External`

The `@External` decorator notifies the compiler that a function is defined in an external library.

```ardium
@External("libc")
extern fn printf(fmt, ...)

fn main() {
    printf("Calling libc printf from Ardium!\n");
}
```

## Linkage

The compiler automatically handles the mapping to external symbols and sets the appropriate calling convention (C-style).
