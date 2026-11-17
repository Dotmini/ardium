# Arc vs Rust vs Go

Machine: arm64 / Darwin 27.0.0  
Best of 3 runs, wall-clock seconds (lower is better). Arc: `arc build` (C backend, -O3). Rust: `rustc -C opt-level=3`. Go: `go build`.

| Benchmark | Workload | Arc | Rust | Go | Arc vs Rust | Arc vs Go | Same output |
|---|---|---:|---:|---:|---:|---:|:---:|
| fib | recursive calls, fib(40) | 0.281 | 0.289 | 0.323 | 0.97× | 0.87× | ✓ |
| sieve | array indexing, primes below 50M | 0.168 | 0.181 | 0.184 | 0.93× | 0.91× | ✓ |
| nbody | float math on structs, 5M steps | 0.197 | 0.206 | 0.215 | 0.96× | 0.92× | ✓ |
| binary_trees | allocation + GC, depth 18 | 1.220 | 1.431 | 1.075 | 0.85× | 1.13× | ✓ |
| wordcount | strings + hash map, 3M inserts | 0.164 | 0.243 | 0.169 | 0.67× | 0.97× | ✓ |
| sort | sort 5M ints | 0.106 | 0.093 | 0.375 | 1.14× | 0.28× | ✓ |

Ratios are Arc time ÷ other time: below 1.00× means Arc was faster.
