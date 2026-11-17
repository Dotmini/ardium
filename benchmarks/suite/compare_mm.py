#!/usr/bin/env python3
"""Before/after check of Arc's memory modes on the benchmark suite.

Builds every workload with --mm=gc and --mm=arc, checks the output against
the reference output recorded by run_suite.py, then runs the new binaries
interleaved (shuffled every round) with the binaries from the last full run
(previous Arc, Rust, Go), so heat and background load affect all equally.
Reports median wall time and peak memory.

    python3 benchmarks/suite/compare_mm.py [--runs 7] [workload ...]
"""
import argparse
import json
import os
import random
import statistics
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.environ.get("SUITE_OUT", "/Volumes/MAC/arc-build/suite")
ARC = os.environ.get("ARC", "/Volumes/MAC/arc-build/arc")
BIN = os.path.join(OUT, "bin_mm")


def measure(path, args):
    """wall seconds and peak RSS (MB) of one run, via wait4"""
    t0 = time.perf_counter()
    pid = os.fork()
    if pid == 0:
        fd = os.open(os.devnull, os.O_WRONLY)
        os.dup2(fd, 1)
        os.execv(path, [path] + args)
    _, status, ru = os.wait4(pid, 0)
    wall = time.perf_counter() - t0
    if status != 0:
        raise RuntimeError(f"{path} exited with {status}")
    return wall, ru.ru_maxrss / (1 << 20)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--runs", type=int, default=7)
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--json", help="also write the medians here")
    ap.add_argument("workloads", nargs="*")
    a = ap.parse_args()
    env = json.load(open(os.path.join(OUT, "results", "environment.json")))
    outputs = json.load(open(os.path.join(OUT, "results", "outputs.json")))
    workloads = a.workloads or list(env["workloads"].keys())
    os.makedirs(BIN, exist_ok=True)
    progs = {}  # (workload, variant) -> path
    for w in workloads:
        src = os.path.join(HERE, w, "main.ar")
        for mm in ("gc", "arc"):
            exe = os.path.join(BIN, f"{w}_{mm}")
            subprocess.run([ARC, "build", "-q", f"--mm={mm}", src, "-o", exe], check=True, capture_output=True)
            got = subprocess.run([exe] + env["workloads"][w], capture_output=True, text=True).stdout
            if got != outputs[f"{w}/arc"]:
                print(f"OUTPUT MISMATCH: {w} --mm={mm}", file=sys.stderr)
                sys.exit(1)
            progs[(w, f"arc-{mm}")] = exe
        for lang in ("arc", "rust", "go"):
            p = os.path.join(OUT, "bin", f"{w}_{lang}")
            if os.path.exists(p):
                progs[(w, "arc-before" if lang == "arc" else lang)] = p
    keys = list(progs)
    for k in keys:  # warm-up: first launch of a new binary is slow on macOS
        measure(progs[k], env["workloads"][k[0]])
    rng = random.Random(a.seed)
    samples = {k: [] for k in keys}
    for r in range(a.runs):
        rng.shuffle(keys)
        for k in keys:
            samples[k].append(measure(progs[k], env["workloads"][k[0]]))
        print(f"  round {r + 1}/{a.runs}", file=sys.stderr, flush=True)
    med = {f"{w}/{v}": {"wall": statistics.median(s[0] for s in samples[(w, v)]),
                        "rss": statistics.median(s[1] for s in samples[(w, v)])} for (w, v) in samples}
    variants = ["arc-before", "arc-gc", "arc-arc", "rust", "go"]
    print(f"{'workload':13}" + "".join(f"{v:>20}" for v in variants))
    for w in workloads:
        cells = []
        for v in variants:
            m = med.get(f"{w}/{v}")
            cells.append(f"{m['wall']:7.3f}s {m['rss']:7.1f}MB" if m else " " * 20)
        print(f"{w:13}" + "".join(f"{c:>20}" for c in cells))
    if a.json:
        json.dump(med, open(a.json, "w"), indent=1)


if __name__ == "__main__":
    main()
