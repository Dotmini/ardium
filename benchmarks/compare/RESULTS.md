# Arc vs Rust vs Go

Machine: arm64 / Darwin 27.0.0  
Best of 3 runs, wall-clock seconds (lower is better). Arc: `arc build` (C backend, -O3). Rust: `rustc -C opt-level=3`. Go: `go build`.

| Benchmark | Workload | Arc | Rust | Go | Arc vs Rust | Arc vs Go | Same output |
|---|---|---:|---:|---:|---:|---:|:---:|
| fib | recursive calls, fib(40) | 0.287 | 0.293 | 0.328 | 0.98× | 0.87× | ✓ |
| sieve | array indexing, primes below 50M | 0.160 | 0.196 | 0.199 | 0.82× | 0.80× | ✓ |
| nbody | float math on structs, 5M steps | 0.202 | 0.209 | 0.217 | 0.97× | 0.93× | ✓ |
| binary_trees | allocation + GC, depth 18 | 1.196 | 1.480 | 1.114 | 0.81× | 1.07× | ✓ |
| wordcount | strings + hash map, 3M inserts | 0.208 | 0.255 | 0.177 | 0.82× | 1.17× | ✓ |
| sort | sort 5M ints | 0.110 | 0.098 | 0.390 | 1.13× | 0.28× | ✓ |

Ratios are Arc time ÷ other time: below 1.00× means Arc was faster.
