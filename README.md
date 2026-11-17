# Arc Programming Language

**Version 3.1.0**
**Architect & Founder:** Tirawat Nantamas (Dotmini Software / SPU AI CLUB)

Arc is a statically typed, compiled programming language with Rust-style safety
and ergonomics and Go-style simplicity: type inference, immutable-by-default
bindings, optionals instead of null, a garbage collector, and native performance
on par with Rust and Go.

```rust
struct Account { owner: str, balance: int }

impl Account {
    fn deposit(mut self, amount: int) { self.balance += amount }
}

fn main() {
    let mut acct = Account { owner: "dot", balance: 0 }
    acct.deposit(150)
    let words = "to be or not to be".split(" ")
    let mut freq: [str: int] = [:]
    for w in words { freq[w] = freq.get_or(w, 0) + 1 }
    println(f"{acct.owner} has {acct.balance}; freq = {freq}")
}
```

## Highlights

- **Native speed.** Arc compiles to C and then to machine code with clang
  (`-O2` for `run`, `-O3` for `build`), on par with Rust and Go (see below).
- **Static types with inference.** `int`, `float`, `bool`, `str`, arrays `[T]`,
  hash maps `[K: V]`, optionals `?T`, structs and enums with `impl` methods, and
  function types. There are no implicit conversions and no null.
- **Enums and pattern matching.** Variants carry data (`Shape::Circle(2.0)`).
  `match` checks that every case is handled and supports guards. It works on
  enums, results, ints, strings and bools, and can be used as a value.
- **Errors as values.** A fallible function returns `!T` and fails with
  `err("...")`. `?` passes the error to the caller, and `ok(v)`/`err(e)`
  patterns, `??` and `unwrap` handle it. `fn main() -> !void` reports an
  escaped error and exits with 1.
- **Closures.** `|x| x * 2` infers its types from context and captures by value.
  Named functions are values too. Arrays have `map`, `filter`, `fold`, `any`,
  `all`, `find`, `sort_by`, `sum`, `min` and `max`.
- **Safe by default.** Bindings are immutable unless declared `let mut`. Arrays
  are bounds-checked, integer division by zero is caught, and unbounded
  recursion is reported as a stack overflow instead of a crash.
- **Automatic memory, two ways.** Pick per build: a generational garbage
  collector (`--mm=gc`, the default, highest throughput) or automatic reference
  counting with a cycle collector (`--mm=arc`, lowest memory). Programs behave
  identically in both. There is no `malloc`/`free` in user code.
- **Helpful errors.** Rust-style diagnostics with source snippets, caret spans,
  and `help:` suggestions, including "did you mean" fixes for typos.
- **Batteries included.** f-strings with format specs, string/array/map methods,
  file I/O, time, random numbers, and a built-in unit-test runner.

## Quick start

Requirements: a C/C++ compiler (`clang`, from Xcode Command Line Tools on macOS) and `make`.

```bash
make build                  # builds ./arc (the C runtime is embedded in it)
./arc doctor                # checks the toolchain end to end
./arc run examples/wordfreq.ar
```

```bash
arc run main.ar [args...]   # compile (cached) and run
arc build main.ar -o app    # standalone optimized executable
arc check main.ar           # type-check only
arc test                    # run @test functions in tests/ and src/
arc new myproject           # scaffold a project (arc.toml, src/, tests/)
arc dev main.ar             # re-run on every save
```

Compiled programs are cached in `~/.cache/arc`, so re-running an unchanged
program starts instantly.

The full language guide (in Thai) is in **[LANGUAGE_REFERENCE.md](LANGUAGE_REFERENCE.md)**.

## Error messages

```text
error: cannot assign twice to immutable variable `count`
  --> main.ar:11:5
   |
11 |     count = count + 1
   |     ^^^^^ cannot assign twice
   |
   = help: consider making this binding mutable: `let mut count` (declared at line 10)

error: no field `nmae` on type `User`
 --> main.ar:5:12
  |
5 |  println(u.nmae)
  |            ^^^^ unknown field
  |
  = help: a field with a similar name exists: `name`
```

Runtime failures point at the source too:

```text
panic: index out of bounds: the len is 3 but the index is 5
  --> main.ar:4:13
```

## Performance

Ten workloads, written with the same algorithm in Arc, Rust, Go, Java and
Python (sources and harness in [`benchmarks/suite/`](benchmarks/suite/)). All
five print byte-identical output. The full, formal comparison is
[`benchmarks/suite/Arc_Benchmark_Report.pdf`](benchmarks/suite/Arc_Benchmark_Report.pdf).

The table below covers both Arc memory modes. Each figure is the median wall
time and peak memory of 9 runs on an Apple M2 (macOS 27). Runs were
interleaved in shuffled order, so heat and background load hit every program
equally. "Arc 1" is the compiler before the generational GC and ARC work.

| Workload | Arc 1 | Arc `--mm=gc` | Arc `--mm=arc` | Rust | Go |
|---|---:|---:|---:|---:|---:|
| fib(38) | 0.118s | 0.117s | 0.119s | 0.122s | 0.140s |
| sieve, 50M | 0.189s | 0.175s | 0.167s | 0.227s | 0.218s |
| nbody, 5M steps | 0.216s | 0.218s | 0.216s | 0.223s | 0.234s |
| binary_trees(18) | 1.410s / 98 MB | **0.680s** / 76 MB | 0.989s / **34 MB** | 1.500s / 18 MB | 1.193s / 39 MB |
| wordcount, 3M | 0.251s / 16 MB | 0.191s / 17 MB | 0.195s / **8 MB** | 0.279s / 7 MB | 0.192s / 11 MB |
| sort, 5M ints | 0.076s | 0.076s | 0.072s | 0.102s | 0.415s |
| matmul 400×400 | 0.039s | 0.037s | 0.037s | 0.040s | 0.034s |
| mandelbrot 2000² | 0.235s | 0.231s | 0.227s | 0.234s | 0.238s |
| fannkuch(10) | 0.180s | 0.180s | 0.178s | 0.163s | 0.173s |
| text, 1M CSV lines | 0.384s / 424 MB | 0.280s / 198 MB | 0.272s / 189 MB | 0.253s / 93 MB | 0.230s / 106 MB |

Run it yourself with `python3 benchmarks/suite/compare_mm.py` after a full
`run_suite.py` run. Things to know:
- **Sort:** Arc's standard sort is a radix sort for numbers, while the other
  languages use comparison sorts. That explains most of the gap.
- **Memory:** Arc still uses more memory than Rust wherever it allocates many
  objects. Every Arc string and object is a separate heap allocation, while
  Rust stores them inline.
- **GC pauses:** the collector is stop-the-world. With the generational
  collector, the longest pause is 7.5 ms on binary_trees and 8.3 ms on text
  (`ARC_GC_STATS=1` prints them). Go's concurrent GC stays under 1 ms.
- **Hashing:** maps use a keyed SipHash, as Rust does, to resist hash flooding.

## Testing

```bash
make test                   # language test suite (tests/run_lang_tests.sh)
```

The suite has four parts:
- **Golden-output programs** (`tests/lang/run/`). Each is built in both memory
  modes. Each mode also runs it with `ARC_GC_STRESS=1`, which collects garbage
  on every allocation and verifies the heap after each collection: no reference
  to a freed object, every write barrier present (GC), and every reference count
  exact (ARC).
- **Compile-error programs** (`tests/lang/errors/`). Each must fail with the
  expected messages.
- **Panic programs** (`tests/lang/panics/`). Each must panic with the expected
  message and exit code.
- **`arc test` checks** (`tests/lang/unit/`).

## Architecture

```text
src/
├── cli/main.cpp          arc command-line tool
├── compiler/
│   ├── lexer.cpp         tokens, string/f-string literals
│   ├── parser.cpp        recursive-descent parser → AST
│   ├── checker.cpp       name resolution, type inference/checking, mutability, diagnostics
│   ├── cgen.cpp          lowering to C (GC-visible temps, write barriers, frame elision)
│   └── driver.cpp        imports, caching, invoking the C compiler
└── runtime/arc_rt.c      generational GC / ARC, strings, arrays, maps, formatting, panics (embedded in arc)
```

Pipeline: source → lexer → parser → type checker → C code generation → `cc -O2/-O3`
→ native executable. The runtime is compiled into each program, so the output
binaries have no dependencies beyond libc.

## Status and roadmap

Implemented: everything in the language reference. Planned next:
- user-defined generics, and interfaces or traits
- module namespaces
- concurrency
- networking (TCP/HTTP)
- C FFI
- an LSP server for editors
- a concurrent GC (pauses are under 10 ms today)
- porting the Cocoa/Metal UI bindings from the previous toolchain

The earlier prototype toolchains are kept for reference: the OCaml/LLVM compiler
in `lib/` and `bin/`, the Titan bytecode VM (`make legacy-vm`), and the old
README in `docs/legacy/`.

## License

The Arc programming language and its toolchain are distributed under the MIT License.
