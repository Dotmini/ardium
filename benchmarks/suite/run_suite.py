#!/usr/bin/env python3
"""Cross-language benchmark suite: Arc, Rust, Go, Java, Python.

Every workload implements the same algorithm with the same input in all five
languages (one directory per workload: main.ar, main.rs, main.go, Main.java,
main.py). The harness:

  build    compile every program (release settings, 3 cold builds each,
           compile time = median) into SUITE_OUT
  verify   run every program once and require byte-identical output
  measure  1 warm-up round + R measured rounds; each round runs every
           (workload, language) pair in a freshly shuffled order; per run it
           records wall time, user/sys CPU time and peak RSS (os.wait4)
  startup  "hello" N times per language, interleaved
  all      everything above, then writes results/*.json

All build products and temporary files go to SUITE_OUT (an external drive by
default). Usage:  python3 run_suite.py all [--rounds 5] [--seed 20261118]
"""
import argparse
import datetime
import json
import math
import os
import platform
import random
import shutil
import statistics
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.environ.get("SUITE_OUT", "/Volumes/MAC/arc-build/suite")
ARC = os.environ.get("ARC", "/Volumes/MAC/arc-build/arc")
PYTHON = os.environ.get("PYTHON") or shutil.which("python3.13") or shutil.which("python3")

WORKLOADS = [
    # name, arguments (identical for every language)
    ("fib", ["38"]),
    ("sieve", ["50000000"]),
    ("nbody", ["5000000"]),
    ("binary_trees", ["18"]),
    ("wordcount", ["3000000"]),
    ("sort", ["5000000"]),
    ("matmul", ["400"]),
    ("mandelbrot", ["2000"]),
    ("fannkuch", ["10"]),
    ("text", ["1000000"]),
]
LANGS = ["arc", "rust", "go", "java", "python"]
SRC = {"arc": "main.ar", "rust": "main.rs", "go": "main.go", "java": "Main.java", "python": "main.py"}


def env_for_builds():
    e = dict(os.environ)
    e["TMPDIR"] = os.path.join(OUT, "tmp")
    e["GOCACHE"] = os.path.join(OUT, "gocache")
    e["GOTMPDIR"] = os.path.join(OUT, "tmp")
    e["GOFLAGS"] = "-trimpath"
    return e


def run_env():
    e = dict(os.environ)
    e["TMPDIR"] = os.path.join(OUT, "tmp")
    return e


def binary(w, lang):
    return os.path.join(OUT, "bin", f"{w}_{lang}")


def command(w, lang, args):
    if lang == "java":
        return ["java", "-cp", os.path.join(OUT, "classes", w), "Main"] + args
    if lang == "python":
        return [PYTHON, os.path.join(HERE, w, "main.py")] + args
    return [binary(w, lang)] + args


# ------------------------------------------------------------------ build --

def build_once(w, lang, nonce):
    """One cold build; returns seconds. Caches that would skip the work are avoided."""
    src = os.path.join(HERE, w, SRC[lang])
    env = env_for_builds()
    if lang == "arc":
        cache = os.path.join(OUT, "tmp", f"arc-cache-{nonce}")
        os.makedirs(cache, mode=0o700)
        env["ARC_CACHE_DIR"] = cache
        cmd = [ARC, "build", "-q", src, "-o", binary(w, lang)]
    elif lang == "rust":
        cmd = ["rustc", "-C", "opt-level=3", "-o", binary(w, lang), src]
    elif lang == "go":
        # copy with a unique comment so Go's build cache cannot reuse the package
        tmpdir = os.path.join(OUT, "src", w)
        os.makedirs(tmpdir, exist_ok=True)
        tmpsrc = os.path.join(tmpdir, "main.go")
        with open(src) as f, open(tmpsrc, "w") as g:
            g.write(f.read() + f"\n// build {nonce}\n")
        cmd = ["go", "build", "-o", binary(w, lang), tmpsrc]
    elif lang == "java":
        cls = os.path.join(OUT, "classes", w)
        shutil.rmtree(cls, ignore_errors=True)
        os.makedirs(cls)
        cmd = ["javac", "-d", cls, src]
    else:
        return None
    t0 = time.perf_counter()
    r = subprocess.run(cmd, env=env, capture_output=True, text=True)
    dt = time.perf_counter() - t0
    if lang == "arc":
        shutil.rmtree(env["ARC_CACHE_DIR"], ignore_errors=True)
    if r.returncode != 0:
        sys.exit(f"build failed: {w}/{lang}\n{r.stdout}\n{r.stderr}")
    return dt


def artifact_size(w, lang):
    if lang == "python":
        return os.path.getsize(os.path.join(HERE, w, "main.py"))
    if lang == "java":
        d = os.path.join(OUT, "classes", w)
        return sum(os.path.getsize(os.path.join(d, f)) for f in os.listdir(d) if f.endswith(".class"))
    return os.path.getsize(binary(w, lang))


def loc(path):
    n = 0
    in_block = False
    for line in open(path, encoding="utf-8"):
        s = line.strip()
        if not s:
            continue
        if in_block:
            if "*/" in s:
                in_block = False
            continue
        if s.startswith("/*"):
            in_block = "*/" not in s
            continue
        if s.startswith("//") or s.startswith("#"):
            continue
        n += 1
    return n


def build_all(builds=3):
    for d in ("bin", "classes", "tmp", "src", "gocache", "results"):
        os.makedirs(os.path.join(OUT, d), exist_ok=True)
    info = {}
    for w, _ in WORKLOADS + [("startup", [])]:
        for lang in LANGS:
            times = []
            for b in range(builds if lang != "python" else 0):
                times.append(build_once(w, lang, f"{w}-{lang}-{b}-{time.time_ns()}"))
            info[f"{w}/{lang}"] = {
                "compile_s": statistics.median(times) if times else None,
                "compile_runs": times,
                "size_bytes": artifact_size(w, lang),
                "loc": loc(os.path.join(HERE, w, SRC[lang])),
            }
            print(f"  built {w:13} {lang:7} {info[f'{w}/{lang}']['compile_s'] or 0:7.3f}s", flush=True)
    return info


# ---------------------------------------------------------------- running --

def run_measured(cmd):
    """Runs cmd; returns (stdout, wall, user, sys, maxrss_bytes, exitcode)."""
    out_path = os.path.join(OUT, "tmp", "stdout.txt")
    with open(out_path, "wb") as out, open(os.devnull, "wb") as err:
        t0 = time.perf_counter()
        p = subprocess.Popen(cmd, stdout=out, stderr=err, env=run_env())
        _, status, ru = os.wait4(p.pid, 0)
        wall = time.perf_counter() - t0
    p.returncode = os.waitstatus_to_exitcode(status)
    with open(out_path, "rb") as f:
        stdout = f.read().decode()
    maxrss = ru.ru_maxrss if platform.system() == "Darwin" else ru.ru_maxrss * 1024
    return stdout, wall, ru.ru_utime, ru.ru_stime, maxrss, p.returncode


def verify():
    outputs = {}
    for w, args in WORKLOADS:
        ref = None
        for lang in LANGS:
            out, wall, *_rest, code = run_measured(command(w, lang, args))
            if code != 0:
                sys.exit(f"{w}/{lang} exited with {code}")
            outputs[f"{w}/{lang}"] = out
            if ref is None:
                ref = out
            elif out != ref:
                sys.exit(f"OUTPUT MISMATCH in {w}: {lang}\n--- arc:\n{ref}\n--- {lang}:\n{out}")
            print(f"  verified {w:13} {lang:7} ({wall:.2f}s)", flush=True)
    return outputs


def measure(rounds, seed):
    rng = random.Random(seed)
    pairs = [(w, args, lang) for w, args in WORKLOADS for lang in LANGS]
    runs = []
    for rnd in range(rounds + 1):  # round 0 = warm-up, discarded
        order = pairs[:]
        rng.shuffle(order)
        for w, args, lang in order:
            _out, wall, user, sysc, rss, code = run_measured(command(w, lang, args))
            if code != 0:
                sys.exit(f"{w}/{lang} failed in round {rnd}")
            runs.append({"round": rnd, "workload": w, "lang": lang, "wall_s": wall, "user_s": user,
                         "sys_s": sysc, "maxrss_bytes": rss})
        print(f"  round {rnd} {'(warm-up) ' if rnd == 0 else ''}done", flush=True)
    return runs


def startup(n, seed):
    rng = random.Random(seed + 1)
    order = [lang for lang in LANGS for _ in range(n + 1)]
    rng.shuffle(order)
    seen = {l: 0 for l in LANGS}
    out = {l: [] for l in LANGS}
    for lang in order:
        seen[lang] += 1
        _o, wall, user, sysc, rss, code = run_measured(command("startup", lang, []))
        if seen[lang] > 1:  # first run of each language is a warm-up
            out[lang].append({"wall_s": wall, "user_s": user, "sys_s": sysc, "maxrss_bytes": rss})
    return out


# ------------------------------------------------------------- statistics --

T975 = {1: 12.706, 2: 4.303, 3: 3.182, 4: 2.776, 5: 2.571, 6: 2.447, 7: 2.365, 8: 2.306, 9: 2.262, 10: 2.228,
        19: 2.093, 20: 2.086, 29: 2.045, 30: 2.042}


def describe(xs):
    n = len(xs)
    mean = statistics.fmean(xs)
    sd = statistics.stdev(xs) if n > 1 else 0.0
    t = T975.get(n - 1, 1.96)
    return {"n": n, "median": statistics.median(xs), "mean": mean, "min": min(xs), "max": max(xs), "stdev": sd,
            "cv_pct": 100 * sd / mean if mean else 0.0, "ci95_half": t * sd / math.sqrt(n) if n > 1 else 0.0}


def summarize(runs):
    measured = [r for r in runs if r["round"] > 0]
    summary = {}
    for w, _ in WORKLOADS:
        summary[w] = {}
        for lang in LANGS:
            rs = [r for r in measured if r["workload"] == w and r["lang"] == lang]
            summary[w][lang] = {
                "wall": describe([r["wall_s"] for r in rs]),
                "cpu": describe([r["user_s"] + r["sys_s"] for r in rs]),
                "maxrss_mb": describe([r["maxrss_bytes"] / 1048576 for r in rs]),
            }
        best = min(summary[w][l]["wall"]["median"] for l in LANGS)
        for lang in LANGS:
            summary[w][lang]["relative"] = summary[w][lang]["wall"]["median"] / best
    geo = {}
    for lang in LANGS:
        rel = [summary[w][lang]["relative"] for w, _ in WORKLOADS]
        geo[lang] = math.exp(sum(math.log(x) for x in rel) / len(rel))
    return summary, geo


def environment(seed, rounds):
    def sh(cmd):
        try:
            return subprocess.run(cmd, capture_output=True, text=True, shell=True).stdout.strip()
        except Exception:
            return ""
    return {
        "date": datetime.datetime.now().isoformat(timespec="seconds"),
        "cpu": sh("sysctl -n machdep.cpu.brand_string"),
        "cores": sh("sysctl -n hw.ncpu"),
        "ram_gb": round(int(sh("sysctl -n hw.memsize") or 0) / 2**30, 1),
        "os": platform.platform(),
        "macos": sh("sw_vers -productVersion"),
        "power": sh("pmset -g batt | head -1"),
        "low_power_mode": sh("pmset -g | grep -i lowpowermode"),
        "load_avg": os.getloadavg(),
        "arc": sh(f"'{ARC}' version"),
        "c_backend": sh("cc --version | head -1"),
        "rustc": sh("rustc --version"),
        "go": sh("go version"),
        "java": sh("java -version 2>&1 | head -2 | tr '\\n' ' '"),
        "python": sh(f"'{PYTHON}' --version"),
        "flags": {
            "arc": "arc build (C backend: cc -std=gnu11 -O3 -fwrapv)",
            "rust": "rustc -C opt-level=3",
            "go": "go build (default gc compiler settings)",
            "java": "javac + java (HotSpot defaults: tiered JIT, G1 GC)",
            "python": "CPython, no flags",
        },
        "seed": seed,
        "rounds": rounds,
        "workloads": {w: args for w, args in WORKLOADS},
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("step", choices=["build", "verify", "measure", "startup", "all"])
    ap.add_argument("--rounds", type=int, default=5)
    ap.add_argument("--seed", type=int, default=20261118)
    ap.add_argument("--startup-runs", type=int, default=20)
    a = ap.parse_args()
    res_dir = os.path.join(OUT, "results")
    os.makedirs(res_dir, exist_ok=True)
    if a.step in ("build", "all"):
        info = build_all()
        json.dump(info, open(os.path.join(res_dir, "build.json"), "w"), indent=1)
    if a.step in ("verify", "all"):
        outs = verify()
        json.dump(outs, open(os.path.join(res_dir, "outputs.json"), "w"), indent=1)
    if a.step in ("startup", "all"):
        st = startup(a.startup_runs, a.seed)
        json.dump(st, open(os.path.join(res_dir, "startup.json"), "w"), indent=1)
    if a.step in ("measure", "all"):
        env = environment(a.seed, a.rounds)
        json.dump(env, open(os.path.join(res_dir, "environment.json"), "w"), indent=1)
        runs = measure(a.rounds, a.seed)
        json.dump(runs, open(os.path.join(res_dir, "runs.json"), "w"), indent=1)
        summary, geo = summarize(runs)
        json.dump({"summary": summary, "geomean_relative": geo}, open(os.path.join(res_dir, "summary.json"), "w"), indent=1)
        env["finished"] = datetime.datetime.now().isoformat(timespec="seconds")
        env["load_avg_end"] = os.getloadavg()
        json.dump(env, open(os.path.join(res_dir, "environment.json"), "w"), indent=1)
        print("geometric mean (relative to the fastest per workload):")
        for lang, g in sorted(geo.items(), key=lambda kv: kv[1]):
            print(f"  {lang:7} {g:8.2f}x")


if __name__ == "__main__":
    main()
