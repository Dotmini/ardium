# Arc vs Rust vs Go

Machine: arm64 / Darwin 27.0.0  
Best of 3 runs, wall-clock seconds (lower is better). Arc: `arc build` (C backend, -O3). Rust: `rustc -C opt-level=3`. Go: `go build`.

| Benchmark | Workload | Arc | Rust | Go | Arc vs Rust | Arc vs Go | Same output |
|---|---|---:|---:|---:|---:|---:|:---:|
| fib | recursive calls, fib(40) | 0.287 | 0.294 | 0.329 | 0.98× | 0.87× | ✓ |
| sieve | array indexing, primes below 50M | 0.157 | 0.188 | 0.193 | 0.84× | 0.82× | ✓ |
| nbody | float math on structs, 5M steps | 0.202 | 0.211 | 0.220 | 0.96× | 0.92× | ✓ |
| binary_trees | allocation + GC, depth 18 | 1.287 | 1.457 | 1.107 | 0.88× | 1.16× | ✓ |
| wordcount | strings + hash map, 3M inserts | 0.208 | 0.248 | 0.174 | 0.84× | 1.20× | ✓ |
| sort | sort 5M ints | 0.108 | 0.094 | 0.380 | 1.15× | 0.28× | ✓ |

Ratios are Arc time ÷ other time: below 1.00× means Arc was faster.
