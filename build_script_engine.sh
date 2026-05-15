#!/bin/bash
set -e

echo "🚀 [Major] Building Titan Script Engine..."

clang++ runtime/src/TitanCompiler.cpp runtime/src/TitanInterpreter.cpp runtime/src/TitanScript.cpp \
    -std=c++20 -O3 -Iruntime/include \
    -o TitanScript

echo "🔹 Created 'TitanScript' Compiler/VM."

# Create a comprehensive test script
cat > final_test.ar <<EOF
fn main() {
    let x = 100;
    println("Starting UI Test");
    VClass {
        Button("Vertical Button 1")
        Button("Vertical Button 2")
    }
    HClass {
        Button("Left")
        Button("Right")
    }
    ZClass {
        Button("Bottom Layer")
        Button("Top Layer")
    }
}
EOF

echo "🔹 Running 'final_test.ar' ભાષા..."
./TitanScript final_test.ar