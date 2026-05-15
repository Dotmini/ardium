#!/bin/bash
set -e

mkdir -p runtime/cli

echo "🚀 [Major] Building Ardium CLI (arc)..."
clang++ runtime/cli/CLI_Main.cpp -std=c++20 -O3 -o arc

echo "✅ 'arc' CLI built successfully."
echo "--------------------------------"
./arc doctor
