# 📘 Ardium (Arc) Language Reference

(Updated for v1.6.0 Ascension)

## 📚 สารบัญ

1. [Introduction](#introduction)
2. [CLI Toolchain (`arc`)](#cli-toolchain)
3. [Syntax & Variables](#syntax--variables)
4. [Control Flow](#control-flow)
5. [Functions & Exports](#functions--exports)
6. [Native Apple GUI](#native-apple-gui)
7. [System & Memory](#system--memory)
8. [Example Code](#example-code)

---

## 1. Introduction <a name="introduction"></a>

Ardium (Compiler Binary: `arc`) เป็นภาษาโปรแกรมระดับ Systems Programming ที่ออกแบบมาเพื่อประสิทธิภาพสูงสุด (High Performance) โดยมีความสามารถพิเศษในการเชื่อมต่อกับ Native Frameworks ของ macOS (Cocoa/AppKit) และ C/Swift ได้โดยตรง

---

## 2. CLI Toolchain (`arc`) <a name="cli-toolchain"></a>

คำสั่งหลักคือ `arc` (เดิมคือ `ar` หรือ `ardium`)

```bash
# รันโปรแกรมทันที (JIT Mode)
arc run main.ar

# คอมไพล์เป็น Executable (Production Build -O3)
arc build main.ar -o myapp

# รันในโหมด Development (Watch Mode - Hot Reload)
arc dev main.ar

# รัน Integration Tests
arc test tests/*.ar

# ดูเวอร์ชัน
arc --version
```

---

## 3. Syntax & Variables <a name="syntax--variables"></a>

### การประกาศตัวแปร (Variables)

Ardium ใช้ keyword `let` สำหรับการประกาศตัวแปร

```rust
// 1. Immutable (ค่าคงที่ - แนะนำให้ใช้เป็นหลัก)
let name = "Ardium";
let version = 1.6;
let is_active = 1;  // 1 = true, 0 = false

// 2. Mutable (เปลี่ยนค่าได้ - ต้องระบุ mut ถ้าต้องการแก้ไข)
let mut count = 0;
count = count + 1;
```

### การตั้งชื่อตัวแปร (Naming Convention)

- **Snake Case**: `my_variable_name` (แนะนำสำหรับตัวแปรทั่วไป)
- **Camel Case**: `myVariableName` (ใช้ได้)
- **Pascal Case**: `MyClass` (แนะนำสำหรับ Class/Struct)
- **ห้าม**: ขึ้นต้นด้วยตัวเลข หรือใช้อักขระพิเศษยกเว้น `_`

### ชนิดข้อมูล (Data Types)

ภาษา Ardium เป็น **Strongly Typed** แต่มี **Type Inference** (ไม่ต้องระบุชนิดตัวแปร บ่อยนัก)

| Type | Description | Example |
|------|-------------|---------|
| `Int` | 64-bit Integer | `42`, `-100`, `0xFF` |
| `Float` | 64-bit Double | `3.14`, `1.0` |
| `String` | UTF-8 String | `"Hello World"` |
| `Pointer` | Generic Pointer (`void*`)| `malloc(64)` |

---

## 4. Control Flow <a name="control-flow"></a>

### If / Else

```rust
if (score > 80) {
    println("Grade A");
} else {
    println("Keep trying");
}
```

### Loops

Ardium ใช้ `loop` keyword (คล้าย while)

```rust
let mut i = 0;
loop (i < 10) {
    print(i);
    i = i + 1;
}
```

---

## 5. Functions & Exports <a name="functions--exports"></a>

### ฟังก์ชันทั่วไป

```rust
fn add(a, b) {
    return a + b;
}

fn main() {
    let sum = add(10, 20);
    println(sum);
}
```

### Export (สำหรับสร้าง Library ให้ภาษาอื่นเรียกใช้)

ใช้ `@export` หรือ `@extern` decorator

```rust
// Export ให้ C/Swift เรียกใช้ได้
@export
fn calculate_magic() {
    return 42;
}
```

### Import C Functions (Extern)

เรียกใช้ฟังก์ชันจาก C Standard Library หรือ Frameworks

```rust
extern fn printf(fmt, ...);
extern fn malloc(size);
extern fn free(ptr);
```

---

## 6. Native Apple GUI <a name="native-apple-gui"></a>

Ardium มี Built-in Bridge สำหรับสร้าง macOS App โดยไม่ต้องเขียน Objective-C

### Core Functions

1. **`init_apple_gui()`**
   - เริ่มต้นระบบ `NSApplication`
   - ทำให้ App icon ปรากฏที่ Dock

2. **`create_apple_window(title, x, y, w, h)`**
   - สร้างหน้าต่างมาตรฐาน
   - **Parameters**: `title` (String), `x`, `y`, `w`, `h` (Integer)

3. **`set_modern_background()`**
   - เปลี่ยนสีพื้นหลังเป็น Dark Mode แบบทันสมัย

4. **`add_styled_text(text)`**
   - เพิ่มข้อความลงบนหน้าต่าง (สำหรับ Test UI)

5. **`run_apple_gui()`**
   - เข้าสู่ Event Loop (Blocking) **ต้องเรียกเป็นคำสั่งสุดท้าย**

### ตัวอย่าง Code สร้าง GUI

```rust
extern fn init_apple_gui();
extern fn create_apple_window(title, x, y, w, h);
extern fn set_modern_background();
extern fn add_styled_text(text);
extern fn run_apple_gui();

fn main() {
    // 1. Init System
    init_apple_gui();

    // 2. Create Window (800x600)
    create_apple_window("My Ardium App", 100, 100, 800, 600);

    // 3. Customize
    set_modern_background();
    add_styled_text("Welcome to Ardium 1.6!");

    // 4. Run Loop
    run_apple_gui();
    
    return 0;
}
```

---

## 7. System & Memory <a name="system--memory"></a>

Ardium ให้คุณจัดการ Memory ได้โดยตรง (Manual Memory Management) ผ่าน Arena Allocator หรือ libc

### Memory Operations

```rust
// จอง Memory
let ptr = malloc(1024);

// เขียนค่าลง Memory (Poke)
poke(ptr, 100);

// อ่านค่าจาก Memory (Peek)
let val = peek(ptr);

// คืน Memory (Optional in Arena mode)
free(ptr);
```

### Global State (`GLOBAL`)

Shared memory ที่เข้าถึงได้จากทุกที่

```rust
GLOBAL(Config) = ("v1.0", 8080);

fn show() {
    println(GLOBAL.Config);
}
```

---
*(C) 2026 Arsenal Engine Project*
