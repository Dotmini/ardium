#!/usr/bin/env python3
"""Arc vs Rust vs Go benchmark harness.

Builds every benchmark in all three languages with release settings, checks
that the outputs are identical, runs each binary several times and reports the
best wall-clock time. Results are printed and written to RESULTS.md.

    python3 benchmarks/compare/run.py            # all benchmarks
    python3 benchmarks/compare/run.py fib sort   # a subset
"""
import os
import platform
import shutil
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
BIN = os.path.join(HERE, "bin")
ARC = os.path.join(ROOT, "arc")
RUNS = int(os.environ.get("RUNS", "3"))

BENCHES = [
    ("fib", "recursive calls, fib(40)"),
    ("sieve", "array indexing, primes below 50M"),
    ("nbody", "float math on structs, 5M steps"),
    ("binary_trees", "allocation + GC, depth 18"),
    ("wordcount", "strings + hash map, 3M inserts"),
    ("sort", "sort 5M ints"),
]


def sh(cmd, **kw):
    return subprocess.run(cmd, check=True, capture_output=True, text=True, **kw)


def build(name):
    os.makedirs(BIN, exist_ok=True)
    src = lambda ext: os.path.join(HERE, f"{name}.{ext}")
    out = {}
    sh([ARC, "build", "-q", src("ar"), "-o", os.path.join(BIN, f"{name}_arc")])
    out["Arc"] = os.path.join(BIN, f"{name}_arc")
    if shutil.which("rustc"):
        sh(["rustc", "-C", "opt-level=3", "-o", os.path.join(BIN, f"{name}_rust"), src("rs")])
        out["Rust"] = os.path.join(BIN, f"{name}_rust")
    if shutil.which("go"):
        sh(["go", "build", "-o", os.path.join(BIN, f"{name}_go"), src("go")], cwd=HERE)
        out["Go"] = os.path.join(BIN, f"{name}_go")
    return out


def timed(path):
    best, output = None, None
    for _ in range(RUNS):
        t0 = time.perf_counter()
        r = subprocess.run([path], capture_output=True, text=True)
        dt = time.perf_counter() - t0
        if r.returncode != 0:
            raise RuntimeError(f"{path} failed: {r.stderr}")
        output = r.stdout
        best = dt if best is None else min(best, dt)
    return best, output


def main():
    selected = [b for b in BENCHES if len(sys.argv) < 2 or b[0] in sys.argv[1:]]
    rows = []
    for name, desc in selected:
        print(f"== {name}: {desc}", flush=True)
        bins = build(name)
        times, outputs = {}, {}
        for lang, path in bins.items():
            times[lang], outputs[lang] = timed(path)
            print(f"   {lang:5} {times[lang]:7.3f}s", flush=True)
        ref = outputs["Arc"]
        mismatch = [l for l, o in outputs.items() if o != ref]
        if mismatch:
            print(f"   OUTPUT MISMATCH in {mismatch}")
            for l, o in outputs.items():
                print(f"   --- {l}:\n{o}")
        rows.append((name, desc, times, not mismatch))

    langs = ["Arc", "Rust", "Go"]
    lines = [
        "# Arc vs Rust vs Go",
        "",
        f"Machine: {platform.machine()} / {platform.system()} {platform.release()}  ",
        f"Best of {RUNS} runs, wall-clock seconds (lower is better). "
        "Arc: `arc build` (C backend, -O3). Rust: `rustc -C opt-level=3`. Go: `go build`.",
        "",
        "| Benchmark | Workload | Arc | Rust | Go | Arc vs Rust | Arc vs Go | Same output |",
        "|---|---|---:|---:|---:|---:|---:|:---:|",
    ]
    for name, desc, t, same in rows:
        cells = [f"{t[l]:.3f}" if l in t else "–" for l in langs]
        rel = lambda other: f"{t['Arc'] / t[other]:.2f}×" if other in t else "–"
        lines.append(f"| {name} | {desc} | {' | '.join(cells)} | {rel('Rust')} | {rel('Go')} | {'✓' if same else '✗'} |")
    lines += ["", "Ratios are Arc time ÷ other time: below 1.00× means Arc was faster.", ""]
    report = "\n".join(lines)
    print()
    print(report)
    with open(os.path.join(HERE, "RESULTS.md"), "w") as f:
        f.write(report)


if __name__ == "__main__":
    main()
