# Implementation Plan - Ardium Evolution (v3.0 - Python-like & Robust)

## Goal Definitions

1. **"No More Segfaults"**: Implement robust error handlers in both the Compiler (OCaml) and Runtime (C++).
    - Compiler: Catch internal exceptions.
    - Runtime: Catch SIGSEGV `139` and print friendly stack traces.
2. **"Like Python" (Flexible Variables)**:
    - Enhance `var` to support dynamic-like typing (or robust type inference).
    - Ensure Syntax is forgiving.
3. **`arc dev`**:
    - Implement a new command `arc dev <file>` that acts as a "Hot Reload" or "Continuous Build" watcher.

## User Review Required
>
> [!IMPORTANT]
> The "Python-like" goal will be interpreted as "Strong Type Inference" and "Forgiving Syntax". Not a full switch to an interpreter.
> `arc dev` will be implemented as a file-watching runner.

## Proposed Changes

### 1. Robustness (Runtime Error Handler)

- **File**: `runtime/src/libardium.cpp`
- **Change**: Add `signal(SIGSEGV, handler)` to catch crashes and print nice messages like:

    ```
    🔥 Ardium Runtime Error: Segmentation Fault
    The memory goblins ate your data.
    ```

### 2. `arc dev` Command

- **File**: `bin/main.ml`
- **Change**: Add `dev` command.
  - Logic: Loop `inotify` or `stat` check on input file -> clear screen -> Compile -> Run.

### 3. Flexible Syntax & Logic

- **File**: `lib/parser.mly`, `lib/codegen.ml`
- **Change**: Ensure `var` always infers type. Mapping unknown types to a universal `Any` if possible (advanced).

## Verification Plan

- **Test Crash**: Run a program that deliberately sets a bad pointer. Check for "Nice Error" instead of "Segmentation fault".
- **Test Dev**: Run `arc dev` and edit a file.
