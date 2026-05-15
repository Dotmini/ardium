# Ardium Syntax Guide

## Variable Declarations

### Immutable Variables (`let`)

Use `let` for variables that should not change.

```ardium
let version = 2.1;
```

### Mutable Variables (`var`)

Use `var` for variables that can be reassigned.

```ardium
var counter = 0;
counter = counter + 1;
```

## Enumerations (`enum`)

Enums allow you to define a type by enumerating its possible variants.

```ardium
enum Color {
    Red,
    Green,
    Blue,
    Custom = 100
}

let choice = Color.Green;
```

## Functions

Define functions using the `fn` or `func` keyword.

```ardium
fn add(a, b) {
    return a + b;
}
```
