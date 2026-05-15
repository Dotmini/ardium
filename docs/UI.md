# UI & Reactivity in Ardium

Ardium provides powerful decorators for building user interfaces and managing state.

## Property Wrappers

### `@State`

Marks a local variable as a source of truth. Changes to `@State` variables trigger UI refreshes.

```ardium
@State
var name = "Ardium";
```

### `@Binding`

Allows a component to read and write a value owned by another component.

## Layout Decorators

Use layout decorators to arrange UI components automatically.

- `@VClass`: Vertical stack.
- `@HClass`: Horizontal stack.
- `@ZClass`: Z-index stack.

```ardium
@VClass
fn MyView() {
    Text("Hello");
    Button("Click Me");
}
```
