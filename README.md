# Arc Programming Language

**Version 3.0.0**
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
- **Automatic memory.** A precise mark-and-sweep garbage collector with a
  size-class allocator. There is no `malloc`/`free` in user code.
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

## Performance: Arc vs Rust vs Go

Six workloads, each written idiomatically in all three languages (all in
[`benchmarks/compare/`](benchmarks/compare/)). Every program prints the same
output in all three languages. Times are the best of 3 runs on Apple Silicon
(arm64, macOS); lower is better.

| Benchmark | Workload | Arc | Rust | Go |
|---|---|---:|---:|---:|
| fib | recursive calls, fib(40) | 0.287s | 0.293s | 0.328s |
| sieve | array indexing, primes below 50M | 0.160s | 0.196s | 0.199s |
| nbody | float math on structs, 5M steps | 0.202s | 0.209s | 0.217s |
| binary_trees | allocation + GC, depth 18 | 1.196s | 1.480s | 1.114s |
| wordcount | strings + hash map, 3M inserts | 0.208s | 0.255s | 0.177s |
| sort | sort 5M ints | 0.110s | 0.098s | 0.390s |

Reproduce the results with `make bench` (needs `rustc` and `go`).

Memory and latency are where a garbage-collected Arc still trails:
- **Peak memory:** binary_trees peaks at 86 MB, against 18 MB for Rust and 35 MB for Go.
- **GC pauses:** the collector is stop-the-world, so pauses grow with heap size. They are about 17 ms at a 60 MB heap, while Go's concurrent GC stays under 1 ms.
- **Hashing:** maps use a keyed SipHash, as Rust does, to resist hash flooding.

## Testing

```bash
make test                   # language test suite (tests/run_lang_tests.sh)
```

The suite has four parts:
- **Golden-output programs** (`tests/lang/run/`). Each is also re-run with
  `ARC_GC_STRESS=1`, which collects garbage on every allocation.
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
│   ├── cgen.cpp          lowering to C (every value lives in a GC-visible temp)
│   └── driver.cpp        imports, caching, invoking the C compiler
└── runtime/arc_rt.c      GC, strings, arrays, maps, formatting, panics (embedded in arc)
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
- an incremental GC
- porting the Cocoa/Metal UI bindings from the previous toolchain

The earlier prototype toolchains are kept for reference: the OCaml/LLVM compiler
in `lib/` and `bin/`, the Titan bytecode VM (`make legacy-vm`), and the old
README in `docs/legacy/`.

## License

The Arc programming language and its toolchain are distributed under the MIT License.
