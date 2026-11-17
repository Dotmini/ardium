# 📘 Arc Language Reference

(Arc 3.x — compiler core ใหม่: static types, native code, garbage collector)

## 📚 สารบัญ

1. [ภาพรวม](#overview)
2. [CLI Toolchain (`arc`)](#cli)
3. [ตัวแปรและชนิดข้อมูล](#variables)
4. [ตัวดำเนินการ](#operators)
5. [Control Flow](#control-flow)
6. [ฟังก์ชัน](#functions)
7. [Struct และ Method](#structs)
8. [Optional (`?T`)](#optionals)
9. [Array](#arrays)
10. [Map](#maps)
11. [String และ f-string](#strings)
12. [Panic, assert และการทดสอบ](#testing)
13. [Modules (`import`)](#modules)
14. [Built-in functions](#builtins)
15. [Memory model](#memory)
16. [สิ่งที่ยังไม่รองรับ](#roadmap)

---

## 1. ภาพรวม <a name="overview"></a>

Arc เป็นภาษาแบบ **statically typed** ที่คอมไพล์เป็น **native code** (ผ่าน C → clang `-O2/-O3`)
มี **type inference**, ตัวแปร **immutable by default** แบบ Rust, หน่วยความจำจัดการอัตโนมัติด้วย
**garbage collector** แบบ Go และข้อความ error ที่บอกตำแหน่งพร้อมคำแนะนำวิธีแก้

```rust
struct Point { x: float, y: float }

impl Point {
    fn len(self) -> float { return sqrt(self.x * self.x + self.y * self.y) }
}

fn main() {
    let p = Point { x: 3, y: 4 }
    println(f"length = {p.len()}")      // length = 5.0
}
```

- คั่น statement ด้วยการขึ้นบรรทัดใหม่ หรือ `;` ก็ได้
- comment: `// ...` และ `/* ... */`

---

## 2. CLI Toolchain (`arc`) <a name="cli"></a>

```bash
arc run main.ar [args...]    # คอมไพล์แล้วรันทันที (binary ถูก cache — รันซ้ำเร็วมาก)
arc build main.ar -o app     # สร้าง executable แบบ standalone (-O3)
arc check main.ar            # ตรวจ type อย่างเดียว ไม่สร้าง binary
arc test                     # รัน @test ทั้งหมดใน tests/ และ src/
arc test file.ar name        # รันเฉพาะ test ที่ชื่อมีคำว่า name
arc new MyProject            # สร้างโปรเจกต์ (arc.toml, src/main.ar, tests/)
arc dev main.ar              # รันใหม่อัตโนมัติเมื่อไฟล์เปลี่ยน
arc app gui.ar -o MyApp      # แพ็กเป็น macOS .app
arc doctor                   # ตรวจ toolchain
arc clean                    # ล้าง build cache (~/.cache/arc)
```

ในโฟลเดอร์ที่มี `arc.toml` สั่ง `arc run` / `arc build` ได้เลยโดยไม่ต้องระบุไฟล์ (ใช้ `src/main.ar`)

ตัวเลือก: `-q` ซ่อน warning, `--emit-c out.c` ดู C ที่ถูกสร้าง, ตัวแปรแวดล้อม `ARC_CC` เปลี่ยน C compiler

Exit code: compile error = `1`, panic = `101`, test fail = `101`

---

## 3. ตัวแปรและชนิดข้อมูล <a name="variables"></a>

```rust
let name = "Arc"          // immutable (แนะนำ)
let mut count = 0         // mutable
var total = 0             // เหมือน let mut
let ratio: float = 1      // ระบุ type เองได้ (int literal กลายเป็น float ให้)

count += 1                // ✓
name = "x"                // ✗ error: cannot assign twice to immutable variable `name`
```

ประกาศซ้ำชื่อเดิมได้ (shadowing): `let x = 5; let x = x * 2`

| Type | ความหมาย | ตัวอย่าง |
|------|----------|---------|
| `int` | จำนวนเต็ม 64-bit | `42`, `-7`, `0xFF`, `0b1010`, `1_000_000` |
| `float` | ทศนิยม 64-bit | `3.14`, `1e9`, `2.5e-3` |
| `bool` | จริง/เท็จ | `true`, `false` |
| `str` | ข้อความ UTF-8 (immutable) | `"hello"`, `f"x = {x}"` |
| `[T]` | array ขยายได้ | `[1, 2, 3]`, `[0; 100]` |
| `[K: V]` | hash map (เรียงตามลำดับที่ใส่) | `["a": 1]`, `[:]` |
| `?T` | optional — มีค่า หรือ `nil` | `let x: ?int = nil` |
| `Name` | struct ที่ประกาศเอง | `Point { x: 1.0, y: 2.0 }` |

ชื่อ alias ที่ใช้ได้: `i64`, `Int` = `int` · `f64`, `Float` = `float` · `string`, `String` = `str`

**ไม่มีการแปลง type อัตโนมัติ** (เหมือน Rust/Go):

```rust
let a = 2
let b = 1.5
a * b              // ✗ error: cannot apply `*` to `int` and `float`
float(a) * b       // ✓ 3.0
a as float * b     // ✓
int(2.9)           // 2 (ตัดทศนิยม)
str(42) + "!"      // "42!"
```

ตัวแปร global ประกาศที่ระดับบนสุดของไฟล์:

```rust
let mut counter = 0
let names = ["a", "b"]

fn bump() { counter += 1 }
```

---

## 4. ตัวดำเนินการ <a name="operators"></a>

| กลุ่ม | ตัวดำเนินการ |
|------|-------------|
| คณิตศาสตร์ | `+ - * / %` (int หาร = ปัดเศษเข้าหาศูนย์, หารด้วย 0 → panic) |
| เปรียบเทียบ | `== != < <= > >=` (`==` เปรียบเทียบค่าแบบลึก รวมถึง array/struct/map) |
| ตรรกะ | `&& \|\| !` (หรือ `and`, `or`, `not`) — short-circuit |
| bit | `& \| ^ ~ << >>` |
| optional | `a ?? b` — ใช้ `b` ถ้า `a` เป็น `nil` |
| แปลง type | `x as float`, `x as int` |
| กำหนดค่า | `= += -= *= /= %= &= \|= ^= <<= >>=` |
| string | `+` ต่อข้อความ (เฉพาะ `str + str`) |

`int` overflow จะ wrap แบบ two's complement (เหมือน Go)

---

## 5. Control Flow <a name="control-flow"></a>

```rust
if score >= 80 {
    println("A")
} elif score >= 70 {          // หรือ `else if`
    println("B")
} else {
    println("C")
}

while n > 0 { n -= 1 }

loop {                        // วนไม่สิ้นสุด จนกว่าจะ break
    if done() { break }
}

for i in 0..10 { }            // 0 ถึง 9
for i in 1..=10 { }           // 1 ถึง 10
for x in [10, 20, 30] { }     // วนสมาชิก array
for i, x in ["a", "b"] { }    // index + ค่า
for k, v in ages { }          // วน map (ตามลำดับที่ใส่)
for ch in "สวัสดี" { }         // วนทีละตัวอักษร (UTF-8)

break / continue
```

เงื่อนไขต้องเป็น `bool` เท่านั้น — `if n { }` เมื่อ `n` เป็น `int` จะ error พร้อมคำแนะนำ `n != 0`

---

## 6. ฟังก์ชัน <a name="functions"></a>

```rust
fn add(a: int, b: int) -> int {
    return a + b
}

fn square(x: float) {          // ไม่ระบุ return type → อนุมานจาก `return`
    return x * x
}

fn greet(name: str) {          // ไม่มี return value
    println("hi " + name)
}

fn fill(mut xs: [int], v: int) {   // `mut` = แก้ไขข้อมูลที่ส่งเข้ามาได้
    for i in 0..xs.len() { xs[i] = v }
}
```

- parameter ต้องระบุ type เสมอ
- เรียกฟังก์ชันที่ประกาศทีหลังได้ / recursion ได้
- ฟังก์ชัน recursive ที่ return ค่า ควรระบุ `-> T`
- `main` ไม่รับ parameter (ใช้ `args()`) และ return ได้ทั้งแบบไม่มีค่า หรือ `int` (เป็น exit code)

---

## 7. Struct และ Method <a name="structs"></a>

```rust
struct Account {
    owner: str,
    balance: int,
    history: [int],
}

impl Account {
    fn open(owner: str) -> Account {             // associated function
        return Account { owner, balance: 0, history: [] }   // `owner` = `owner: owner`
    }
    fn describe(self) -> str {                   // method อ่านอย่างเดียว
        return f"{self.owner}: {self.balance}"
    }
    fn deposit(mut self, amount: int) {          // method ที่แก้ไข self
        self.balance += amount
        self.history.push(amount)
    }
}

fn main() {
    let mut acct = Account::open("dot")
    acct.deposit(100)
    println(acct.describe())     // dot: 100
    println(acct)                // Account { owner: "dot", balance: 100, history: [100] }
}
```

- ทุก field ต้องกำหนดค่าตอนสร้าง (ลืม → error บอกชื่อ field)
- การเรียก method ที่เป็น `mut self` ต้องใช้ตัวแปร `let mut`
- struct เป็น **reference type**: `let q = p` ทำให้ `q` และ `p` ชี้ object เดียวกัน

struct ที่อ้างถึงตัวเอง (linked list / tree) ใช้ optional:

```rust
struct Tree { value: int, left: ?Tree, right: ?Tree }
```

---

## 8. Optional (`?T`) <a name="optionals"></a>

ค่าที่ "อาจไม่มี" ต้องเป็น type `?T` เท่านั้น — ไม่มี null pointer ในภาษา

```rust
fn find(xs: [str], target: str) -> ?int {
    for i, x in xs { if x == target { return i } }
    return nil
}

let r = find(names, "ben")

if let i = r {                 // แกะค่าอย่างปลอดภัย
    println("found at", i)
} else {
    println("missing")
}

let idx = r ?? -1              // ค่า default
r.unwrap()                     // ได้ค่า หรือ panic ถ้าเป็น nil
r.unwrap_or(0)
r.is_some() / r.is_nil()
r == nil
```

ค่า `T` ใส่ในตำแหน่งที่ต้องการ `?T` ได้เลย (`let x: ?int = 5`)

---

## 9. Array <a name="arrays"></a>

```rust
let mut xs = [5, 3, 8]
let zeros = [0; 100]                 // 100 ช่อง ค่า 0
let empty: [str] = []                // array ว่างต้องระบุ type

xs.push(1)        xs.pop()           xs.insert(0, 9)      xs.remove(1)
xs.len()          xs.is_empty()      xs.first()           xs.last()
xs.contains(8)    xs.index_of(8)     xs.sort()            xs.reverse()
xs.clear()        xs.copy()          xs[1..3]             xs[2..]   xs[..2]
["a", "b"].join(", ")
```

- index นอกขอบเขต → panic พร้อมบอก len/index/ตำแหน่ง
- method ที่แก้ไข array (`push`, `sort`, ...) ต้องใช้กับตัวแปร `let mut`

---

## 10. Map <a name="maps"></a>

```rust
let mut ages = ["alice": 30, "bob": 25]
let mut counts: [str: int] = [:]      // map ว่าง

ages["carol"] = 35                    // เพิ่ม/แก้
ages["alice"] += 1
ages["zed"]                           // ✗ panic: key not found in map: "zed"
ages.get("zed")                       // nil  (type ?int)
ages.get_or("zed", 0)                 // 0
ages.contains("bob")                  // true
ages.remove("bob")                    // true
ages.keys()    ages.values()    ages.len()    ages.clear()

for name, age in ages { println(name, age) }
```

key ได้แค่ `int`, `str`, `bool` · การวนลูปเรียงตามลำดับที่ใส่ (deterministic)

---

## 11. String และ f-string <a name="strings"></a>

```rust
let s = "Hello, World"
s.len()            // 12 (bytes)
s.char_count()     // จำนวนตัวอักษร (UTF-8)
s.upper()  s.lower()  s.trim()
s.contains("World")  s.starts_with("He")  s.ends_with("ld")  s.find("o")  // -1 ถ้าไม่เจอ
s.split(",")  s.split_whitespace()  s.lines()  s.chars()
s.replace("l", "L")  s.repeat(3)
s[0..5]  s[7..]                     // slice (byte index)
s.byte_at(0)                        // 72
s.to_int()  s.to_float()            // ?int / ?float
ord("A")  chr(66)                   // 65, "B"
```

f-string — ใส่ expression ใน `{ }` ได้ทุกชนิด พร้อม format spec:

```rust
let name = "Arc"
let pi = 3.14159
f"Hello {name}!"            // Hello Arc!
f"{pi:.2}"                  // 3.14
f"[{42:5}]"                 // [   42]   (ตัวเลขชิดขวา)
f"[{name:<6}]"              // [Arc   ]
f"[{name:^7}]"              // [  Arc  ]
f"[{7:03}]"                 // [007]
f"[{name:*>6}]"             // [***Arc]
f"{{literal braces}}"
```

escape: `\n \t \r \\ \" \0 \e \x41 \u{1F600} \{ \}`

---

## 12. Panic, assert และการทดสอบ <a name="testing"></a>

```rust
panic("something went wrong")
assert(x > 0)                       // panic: assertion failed: x > 0
assert(x > 0, "x must be positive")
assert_eq(add(2, 2), 4)             // แสดง left/right เมื่อไม่เท่ากัน
exit(2)
```

panic จะพิมพ์ข้อความ + ตำแหน่ง (`--> file.ar:12:9`) และจบด้วย exit code 101
การ recursion ไม่สิ้นสุดจะถูกจับเป็น `fatal: stack overflow`

**Unit tests** — ใส่ `@test` หน้าฟังก์ชัน แล้วรัน `arc test`:

```rust
@test
fn test_add() {
    assert_eq(add(2, 3), 5)
}
```

```
running 1 test
test test_add ... ok

test result: ok. 1 passed; 0 failed; 0 filtered out
```

---

## 13. Modules (`import`) <a name="modules"></a>

```rust
// utils.ar
fn triple(x: int) -> int { return x * 3 }

// main.ar
import "utils"            // โหลด utils.ar จากโฟลเดอร์เดียวกัน
fn main() { println(triple(14)) }
```

ทุกไฟล์ที่ import จะใช้ namespace ร่วมกัน (import ซ้ำได้ ไม่โหลดซ้ำ)
`import "Core"` และ framework เดิมอื่น ๆ ไม่จำเป็นแล้ว (built-in ทั้งหมด) — ถูกข้ามโดยอัตโนมัติ

---

## 14. Built-in functions <a name="builtins"></a>

| หมวด | ฟังก์ชัน |
|------|---------|
| แสดงผล | `print(...)`, `println(...)`, `eprint(...)`, `eprintln(...)` — รับหลายค่า คั่นด้วยช่องว่าง |
| แปลง type | `str(x)`, `int(x)`, `float(x)`, `parse_int(s) -> ?int`, `parse_float(s) -> ?float` |
| ขนาด | `len(x)` (str/array/map) |
| คณิตศาสตร์ | `abs`, `min`, `max`, `sqrt`, `pow`, `floor`, `ceil`, `round`, `sin`, `cos`, `tan`, `atan2`, `exp`, `log`, `log10`, `ipow(int, int)` |
| ข้อความ | `chr(code)`, `ord(s)` |
| ไฟล์ / ระบบ | `read_line() -> ?str`, `read_file(path) -> ?str`, `write_file(path, s) -> bool`, `append_file`, `file_exists`, `args() -> [str]`, `getenv(name) -> ?str`, `run(cmd) -> int` |
| เวลา | `time_ns()`, `time_ms()`, `unix_time()`, `sleep_ms(ms)` |
| สุ่ม | `rand_int(lo, hi)`, `rand_float()`, `seed(n)` |
| ควบคุม | `panic(msg)`, `assert(c)`, `assert_eq(a, b)`, `exit(code)` |

ประกาศฟังก์ชันชื่อเดียวกับ built-in ได้ — ฟังก์ชันของเราจะถูกใช้แทน

---

## 15. Memory model <a name="memory"></a>

- `int`, `float`, `bool` เป็น value type (copy เมื่อกำหนดค่า)
- `str` เป็น immutable — แชร์ได้อย่างปลอดภัย
- array, map, struct เป็น reference type — ส่งเข้าฟังก์ชันไม่ copy (ถ้าต้องการสำเนาใช้ `.copy()`)
- หน่วยความจำคืนอัตโนมัติด้วย precise mark & sweep GC (size-class allocator) ไม่มี `malloc`/`free`
- ทดสอบ GC แบบเข้มได้ด้วย `ARC_GC_STRESS=1 arc run file.ar` (เก็บขยะทุกครั้งที่ allocate)

---

## 16. สิ่งที่ยังไม่รองรับ <a name="roadmap"></a>

แผนถัดไป: `enum` + `match`, generics, closures/lambda, `Result`-style error handling,
module namespaces, concurrency (threads/channels), FFI กับ C, และ GUI/Metal bindings ของเดิม
(`VStack`, `Button`, `extern fn`, `GLOBAL(...)`, `malloc/peek/poke` ของคอมไพเลอร์รุ่นเก่ายังไม่ถูกย้ายมา)

---
*(C) 2026 Arc Language Project — Dotmini Software / SPU AI CLUB*
