# AI Prompt: Ardium Performance Optimization Phase 3

## Task

Implement LLVM `opt` integration to improve Ardium compiler performance from 0.38s to 0.15s.

## Current State

- Performance: 0.38s (Rust: 0.09s)
- Phase 1 Complete: Type inference
- Compiler flags: `-O3 -flto -funroll-loops -march=native`

## Implementation

### Modify `bin/main.ml` after line 235

```ocaml
(* Emit and optimize LLVM IR *)
let ir_file = Filename.temp_file "ardium_" ".ll" in
Llvm.print_module ir_file the_module;

let opt_ir = Filename.temp_file "ardium_opt_" ".ll" in
let opt_cmd = Printf.sprintf 
  "opt -O3 -loop-unroll -loop-vectorize -slp-vectorize \
   -licm -gvn -sccp -dce -inline %s -o %s" 
  ir_file opt_ir in
let opt_result = Sys.command opt_cmd in

let ir_to_compile = if opt_result = 0 then opt_ir else ir_file in
let llc_cmd = Printf.sprintf "llc -O3 -filetype=obj %s -o %s" 
  ir_to_compile obj_file in
ignore (Sys.command llc_cmd);

Sys.remove ir_file;
if opt_result = 0 then Sys.remove opt_ir;
```

## Test

```bash
make build && ar build bench.ar -o bench && time ./bench
```

**Expected**: 0.15-0.20s

## Files

- `/Users/dotmini/Documents/ardium/bin/main.ml`
- `/Users/dotmini/Documents/ardium/bench.ar`
- `/Users/dotmini/Documents/ardium/OPTIMIZATION_GUIDE.md`
