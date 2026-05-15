# Ardium Compiler - Rust-Level Performance Optimization

## Current Status (Phase 1 Complete)

### What Has Been Done

1. ✅ **Type Inference System** - Created `lib/type_inference.ml` with Hindley-Milner style inference
2. ✅ **Integrated into Codegen** - Modified `lib/codegen.ml` to use inferred types
3. ✅ **Compiler Optimizations** - Added `-O3 -flto -funroll-loops -march=native` flags
4. ✅ **Fixed Segfaults** - Resolved runtime crashes in IO functions and memory allocation

### Current Performance

- **Ardium**: 0.38s for 100M loop iterations
- **Target (Rust/C++)**: 0.09s
- **Gap**: 4x slower

---

## Files Modified

### 1. lib/type_inference.ml (NEW)

**Purpose**: Static type inference to eliminate runtime conversions

**Key Functions**:

- `infer_expr`: Infer types from expressions
- `infer_stmt`: Infer types from statements  
- `infer_program`: Run inference on entire program
- `inferred_to_llvm_type`: Convert inferred types to LLVM types

### 2. lib/codegen.ml (MODIFIED)

**Changes**:

- Added `type_env` field to `Context.t`
- Added `Types.inferred_to_llvm_type` helper
- Modified `Let` statement codegen to use inferred types
- Added type inference pass before codegen (line 931-936)

### 3. bin/main.ml (MODIFIED)

**Changes**:

- Updated clang flags: `-O3 -flto -funroll-loops -march=native`
- Applied to all build modes (run, build, lib)

---

## Next Steps: Phase 3 - LLVM Opt Integration

### Goal

Achieve 0.12-0.18s performance (within 2x of Rust) by using LLVM's `opt` tool.

### Implementation Plan

#### Step 1: Emit LLVM IR to File

Modify `bin/main.ml` after line 235 (after `Codegen.codegen_program`):

```ocaml
(* Emit LLVM IR *)
let ir_file = Filename.temp_file "ardium_" ".ll" in
Llvm.print_module ir_file the_module;
```

#### Step 2: Run opt Tool

Add after IR emission:

```ocaml
(* Run LLVM opt with aggressive passes *)
let opt_ir = Filename.temp_file "ardium_opt_" ".ll" in
let opt_cmd = Printf.sprintf 
  "opt -O3 \
   -loop-unroll -loop-unroll-threshold=1000 \
   -loop-vectorize -slp-vectorize \
   -licm -gvn -sccp -dce -inline \
   -instcombine -reassociate \
   %s -o %s" ir_file opt_ir in
let opt_result = Sys.command opt_cmd in
if opt_result <> 0 then
  Printf.eprintf "Warning: opt failed, using unoptimized IR\n";

(* Use optimized IR for compilation *)
let ir_to_compile = if opt_result = 0 then opt_ir else ir_file in
```

#### Step 3: Compile Optimized IR

Replace object file generation with:

```ocaml
(* Compile IR to object file *)
let llc_cmd = Printf.sprintf "llc -O3 -filetype=obj %s -o %s" 
  ir_to_compile obj_file in
ignore (Sys.command llc_cmd);

(* Clean up temp files *)
Sys.remove ir_file;
if opt_result = 0 then Sys.remove opt_ir;
```

### Expected Performance Improvement

- **Loop Unrolling**: 10-15% faster
- **Vectorization (SIMD)**: 20-30% faster  
- **GVN + Dead Code**: 10-15% faster
- **Total**: 40-60% faster → **0.15-0.23s**

---

## Alternative: Use Clang's Built-in Optimization

If `opt` is not available, modify clang flags in `bin/main.ml`:

```ocaml
(* Add these flags to clang command *)
let opt_flags = "-O3 -flto -march=native \
  -ffast-math -funroll-loops \
  -fvectorize -fslp-vectorize \
  -mllvm -inline-threshold=1000"
```

---

## Testing Commands

```bash
# Build compiler
make build

# Test with benchmark
ar build bench.ar -o bench
time ./bench

# Expected output
109999999.831761
./bench  0.15s user 0.00s system 99% cpu 0.151 total
```

---

## Phase 4: Loop-Specific Optimizations (Optional)

### Add Loop Metadata

In `lib/codegen.ml`, modify `While` codegen (line 775):

```ocaml
| While (cond, body) ->
    let the_func = block_parent (insertion_block ctx.Context.builder) in
    let cond_bb = append_block ctx.Context.llvm_ctx "while_cond" the_func in
    let loop_bb = append_block ctx.Context.llvm_ctx "while_body" the_func in
    let after_bb = append_block ctx.Context.llvm_ctx "while_end" the_func in
    
    (* Add loop metadata for optimization hints *)
    let md_ctx = Llvm.mdkind_id ctx.Context.llvm_ctx "llvm.loop" in
    let md_unroll = Llvm.mdstring ctx.Context.llvm_ctx "llvm.loop.unroll.enable" in
    let md_vec = Llvm.mdstring ctx.Context.llvm_ctx "llvm.loop.vectorize.enable" in
    
    ignore (build_br cond_bb ctx.Context.builder);
    position_at_end cond_bb ctx.Context.builder;
    let cond_val = Convert.to_bool ctx (Expr.codegen ctx cond) in
    ignore (build_cond_br cond_val loop_bb after_bb ctx.Context.builder);
    
    position_at_end loop_bb ctx.Context.builder;
    List.iter (codegen ctx func_val) body;
    
    if block_terminator (insertion_block ctx.Context.builder) = None then
      let br = build_br cond_bb ctx.Context.builder in
      (* Attach metadata to branch instruction *)
      set_metadata br md_ctx (Llvm.mdnode ctx.Context.llvm_ctx [md_unroll; md_vec]);
    
    position_at_end after_bb ctx.Context.builder
```

---

## Troubleshooting

### If opt is not found

```bash
# Install LLVM tools
brew install llvm

# Add to PATH
export PATH="/opt/homebrew/opt/llvm/bin:$PATH"
```

### If performance doesn't improve

1. Check generated IR: `ar build --emit-ir bench.ar`
2. Inspect with: `opt -analyze -view-cfg bench.ll`
3. Verify vectorization: `llc -debug-only=loop-vectorize bench.ll`

---

## Final Performance Target

| Language | Time | Status |
|----------|------|--------|
| Rust/C++ | 0.09s | Target |
| **Ardium (Current)** | **0.38s** | ✅ Phase 1 |
| **Ardium (Phase 3)** | **0.15s** | 🎯 Next |
| **Ardium (Phase 4)** | **0.10s** | 🚀 Goal |
| Python | 4.18s | Baseline |

---

## Summary for AI Assistant

**Context**: Ardium is a custom programming language compiler built with OCaml and LLVM. Current performance is 4x slower than Rust/C++.

**Completed**:

- Type inference system
- Compiler optimization flags
- Fixed runtime segfaults

**Next Task**: Integrate LLVM `opt` tool to apply aggressive loop optimizations (unrolling, vectorization, GVN).

**Expected Result**: Reduce execution time from 0.38s to 0.15s (within 2x of Rust).

**Key Files**:

- `bin/main.ml` - CLI and build pipeline
- `lib/codegen.ml` - LLVM IR generation
- `lib/type_inference.ml` - Static type inference

**Command to Test**:

```bash
ar build bench.ar -o bench && time ./bench
```
