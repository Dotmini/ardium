# Arc vs Rust vs Go

Machine: arm64 / Darwin 27.0.0  
Best of 3 runs, wall-clock seconds (lower is better). Arc: `arc build` (C backend, -O3). Rust: `rustc -C opt-level=3`. Go: `go build`.

| Benchmark | Workload | Arc | Rust | Go | Arc vs Rust | Arc vs Go | Same output |
|---|---|---:|---:|---:|---:|---:|:---:|
| fib | recursive calls, fib(40) | 0.284 | 0.291 | 0.325 | 0.98× | 0.88× | ✓ |
| sieve | array indexing, primes below 50M | 0.159 | 0.187 | 0.192 | 0.85× | 0.83× | ✓ |
| nbody | float math on structs, 5M steps | 0.199 | 0.207 | 0.216 | 0.96× | 0.92× | ✓ |
| binary_trees | allocation + GC, depth 18 | 1.274 | 1.443 | 1.096 | 0.88× | 1.16× | ✓ |
| wordcount | strings + hash map, 3M inserts | 0.203 | 0.244 | 0.164 | 0.83× | 1.23× | ✓ |
| sort | sort 5M ints | 0.107 | 0.093 | 0.377 | 1.15× | 0.28× | ✓ |

Ratios are Arc time ÷ other time: below 1.00× means Arc was faster.
