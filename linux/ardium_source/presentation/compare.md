# 🚀 Ardium Comparative Results (True LLVM JIT)

Generated with **Ardium JIT Execution Engine** and LITE Runtime.

## Performance Rankings

| Topic | Winner | Ardium (s) | Go (s) | Rust (s) |
|:---|:---|:---:|:---:|:---:|
| 01: Basics | ⚡️ **Ardium** | **0.043** | 0.601 | 0.845 |
| 02: Recursion | ⚡️ **Ardium** | **0.059** | 0.878 | 0.644 |
| 03: Structs | ⚡️ **Ardium** | **0.097** | 0.630 | 0.624 |
| 04: Arrays | ⚡️ **Ardium** | **0.125** | 0.934 | 0.577 |
| 05: Strings | ⚡️ **Ardium** | **0.116** | 0.662 | 0.708 |
| 06: FileIO | ⚡️ **Ardium** | **0.096** | 0.671 | 0.666 |
| 07: JSON | ⚡️ **Ardium** | **0.057** | 0.716 | 0.607 |
| 08: Concurrency | ⚡️ **Ardium** | **0.160** | 1.603 | 1.594 |
| 11: Benchmark | ⚡️ **Ardium** | **0.357** | 0.850 | 1.019 |

## Topic 11: Benchmark (Sieve 1,000,000)

| Language | Total Time (s) | Status | Result |
|:---|:---:|:---:|:---|
| ⚡️ **Ardium (JIT)** | **0.357** | ✅ Pass | 78498 Primes |
| 🐹 **Go** | 0.850 | ✅ Pass | 78498 Primes |
| 🦀 **Rust** | 1.019 | ✅ Pass | 78498 Primes |
| 🐍 **Python** | 4.520 | ✅ Pass | 78498 Primes |

> [!IMPORTANT]
> **Ardium JIT** achieves near-zero cold-start latency by executing LLVM IR directly in memory, bypassing disk I/O and external linking. This makes it **2x to 10x faster** than Go and Rust for rapid developer cycles.
