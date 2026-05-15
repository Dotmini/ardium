#!/bin/bash

# Ardium Benchmark Runner
# Compiles and runs Sieve of Eratosthenes across 6 languages

echo "========================================"
echo "🚀 SPU AI CLUB: Ardium Performance Benchmark"
echo "========================================"
echo "Task: Sieve of Eratosthenes (N=1,000,000)"
echo "CPU: Apple Silicon (Optimized Build)"
echo "----------------------------------------"

# 1. Ardium
echo "Compiling Ardium..."
# We use 'arc build' which now uses -O3
# Input: benchmark.ar -> Output: benchmark_ar
../../_build/default/bin/main.exe build benchmark.ar -o benchmark_ar

echo -n "👉 Ardium:     "
/usr/bin/time -p ./benchmark_ar 2>&1 | grep real | awk '{print $2 "s"}'

# 2. Rust
echo "Compiling Rust..."
rustc -O benchmark.rs -o benchmark_rs

echo -n "👉 Rust:       "
/usr/bin/time -p ./benchmark_rs 2>&1 | grep real | awk '{print $2 "s"}'

# 3. C / Obj-C
echo "Compiling Obj-C..."
clang -O3 benchmark.m -o benchmark_m -framework Foundation

echo -n "👉 Obj-C:      "
/usr/bin/time -p ./benchmark_m 2>&1 | grep real | awk '{print $2 "s"}'

# 4. Swift
echo "Compiling Swift..."
swiftc -O benchmark.swift -o benchmark_swift

echo -n "👉 Swift:      "
/usr/bin/time -p ./benchmark_swift 2>&1 | grep real | awk '{print $2 "s"}'

# 5. Go
echo "Compiling Go..."
go build -o benchmark_go benchmark.go

echo -n "👉 Go:         "
/usr/bin/time -p ./benchmark_go 2>&1 | grep real | awk '{print $2 "s"}'

# 6. Python
echo "Running Python..."
echo -n "👉 Python:     "
/usr/bin/time -p python3 benchmark.py 2>&1 | grep real | awk '{print $2 "s"}'

echo "----------------------------------------"
echo "✅ Benchmark Complete"

# Cleanup
rm -f benchmark_ar benchmark_ar.o benchmark_rs benchmark_m benchmark_swift benchmark_go
