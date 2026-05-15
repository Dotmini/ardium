#!/bin/bash
set -e

echo "Compiling CoreUI Demo..."
# Compile .ar to object file (assuming ardium outputs output.o by default or we can specify)
# Based on Makefile: dune exec -- ardium test.ar seems to generate test.o ?
# Let's try to assume it generates coreui_demo.o if passed input is coreui_demo.ar, or standard output.

# 1. Compile Ardium code to LLVM IR / Object using local binary
# The bin name is 'main' internally, public name 'arc'.
# dune exec -- arc might work if installed? But simpler to run main.exe directly.
# Default CLI seems to be: arc [subcommand] [file]
# Based on Makefile: ardium run $(FILE)

./_build/default/bin/main.exe coreui_simple.ar

# Assuming the compiler output is `output.o` or `coreui_demo.o`
# Let's check what it produces. If it produces `coreui_demo.o`, great.
# CodeGen usually produces output.ll, then llc -> output.s -> as -> output.o?
# Or the compiler driver handles it.
# I'll check the directory after running ardium.

# 2. Link with libCoreUI
echo "Linking..."
clang++ output.o lib/libCoreUI.a -o coreui_app \
    -framework Metal \
    -framework Cocoa \
    -framework QuartzCore \
    -framework Foundation

echo "Running CoreUI App..."
./coreui_app
