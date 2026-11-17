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
  hash maps `[K: V]`, optionals `?T`, and structs with `impl` methods. There are
  no implicit conversions and no null.
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
| fib | recursive calls, fib(40) | 0.281s | 0.289s | 0.323s |
| sieve | array indexing, primes below 50M | 0.168s | 0.181s | 0.184s |
| nbody | float math on structs, 5M steps | 0.197s | 0.206s | 0.215s |
| binary_trees | allocation + GC, depth 18 | 1.220s | 1.431s | 1.075s |
| wordcount | strings + hash map, 3M inserts | 0.164s | 0.243s | 0.169s |
| sort | sort 5M ints | 0.106s | 0.093s | 0.375s |

Reproduce the results with `make bench` (needs `rustc` and `go`).

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

Implemented: everything in the language reference. Planned next: `enum` + `match`,
generics, closures, `Result`-style error handling, module namespaces,
concurrency, C FFI, and porting the Cocoa/Metal UI bindings from the previous
toolchain.

The earlier prototype toolchains are kept for reference: the OCaml/LLVM compiler
in `lib/` and `bin/`, the Titan bytecode VM (`make legacy-vm`), and the old
README in `docs/legacy/`.

## License

The Arc programming language and its toolchain are distributed under the MIT License.
