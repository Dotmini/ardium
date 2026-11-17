import subprocess
import time
import os
import statistics

# Set up environment
ENV = os.environ.copy()
HOME = os.path.expanduser("~")
ARC_BIN = os.path.join(HOME, ".cargo/bin/arc")

BENCHMARKS = [
    {
        "id": 1,
        "name": "Prime Count (Trial Division 2..15,000)",
        "arc": """
let limit = 15000;
let count = 0;
let n = 2;
let d = 2;
let is_prime = 1;

fn main() {
    while (n <= limit) {
        is_prime = 1;
        d = 2;
        while (d * d <= n) {
            if (n % d == 0) {
                is_prime = 0;
                d = n;
            } else {
                d = d + 1;
            }
        }
        if (is_prime == 1) {
            count = count + 1;
        }
        n = n + 1;
    }
    println(count);
}
""",
        "py": """
limit = 15000
count = 0
n = 2
while n <= limit:
    is_prime = 1
    d = 2
    while d * d <= n:
        if n % d == 0:
            is_prime = 0
            break
        d += 1
    if is_prime == 1:
        count += 1
    n += 1
print(count)
""",
        "swift": """
let limit = 15000
var count = 0
var n = 2
while n <= limit {
    var is_prime = 1
    var d = 2
    while d * d <= n {
        if n % d == 0 {
            is_prime = 0
            break
        }
        d += 1
    }
    if is_prime == 1 {
        count += 1
    }
    n += 1
}
print(count)
"""
    },
    {
        "id": 2,
        "name": "Collatz Total Steps (1..20,000)",
        "arc": """
let total_steps = 0;
let i = 1;
let n = 1;
let steps = 0;

fn main() {
    while (i <= 20000) {
        n = i;
        steps = 0;
        while (n != 1) {
            if (n % 2 == 0) {
                n = n / 2;
            } else {
                n = (3 * n) + 1;
            }
            steps = steps + 1;
        }
        total_steps = total_steps + steps;
        i = i + 1;
    }
    println(total_steps);
}
""",
        "py": """
total_steps = 0
i = 1
while i <= 20000:
    n = i
    steps = 0
    while n != 1:
        if n % 2 == 0:
            n = n // 2
        else:
            n = 3 * n + 1
        steps += 1
    total_steps += steps
    i += 1
print(total_steps)
""",
        "swift": """
var total_steps: Int64 = 0
var i: Int64 = 1
while i <= 20000 {
    var n = i
    var steps: Int64 = 0
    while n != 1 {
        if n % 2 == 0 {
            n = n / 2
        } else {
            n = 3 * n + 1
        }
        steps += 1
    }
    total_steps += steps
    i += 1
}
print(total_steps)
"""
    },
    {
        "id": 3,
        "name": "Modular Exponentiation (100,000 ops)",
        "arc": """
let base = 7;
let exp = 256;
let mod = 1000000007;
let checksum = 0;
let iter = 0;
let res = 1;
let b = 7;
let e = 256;

fn main() {
    while (iter < 100000) {
        res = 1;
        b = base;
        e = exp;
        while (e > 0) {
            if (e % 2 == 1) {
                res = (res * b) % mod;
            }
            b = (b * b) % mod;
            e = e / 2;
        }
        checksum = (checksum + res) % mod;
        iter = iter + 1;
    }
    println(checksum);
}
""",
        "py": """
base = 7
exp = 256
mod = 1000000007
checksum = 0
iter_count = 0
while iter_count < 100000:
    res = 1
    b = base
    e = exp
    while e > 0:
        if e % 2 == 1:
            res = (res * b) % mod
        b = (b * b) % mod
        e = e // 2
    checksum = (checksum + res) % mod
    iter_count += 1
print(checksum)
""",
        "swift": """
let base: Int64 = 7
let exp: Int64 = 256
let mod: Int64 = 1000000007
var checksum: Int64 = 0
var iter_count = 0
while iter_count < 100000 {
    var res: Int64 = 1
    var b = base
    var e = exp
    while e > 0 {
        if e % 2 == 1 {
            res = (res * b) % mod
        }
        b = (b * b) % mod
        e = e / 2
    }
    checksum = (checksum + res) % mod
    iter_count += 1
}
print(checksum)
"""
    },
    {
        "id": 4,
        "name": "Fibonacci Modulo (Loop N=2,000,000)",
        "arc": """
let n = 2000000;
let a = 0;
let b = 1;
let c = 0;
let i = 2;
let mod = 1000000007;

fn main() {
    while (i <= n) {
        c = (a + b) % mod;
        a = b;
        b = c;
        i = i + 1;
    }
    println(b);
}
""",
        "py": """
n = 2000000
a = 0
b = 1
i = 2
mod = 1000000007
while i <= n:
    c = (a + b) % mod
    a = b
    b = c
    i += 1
print(b)
""",
        "swift": """
let n = 2000000
var a: Int64 = 0
var b: Int64 = 1
var i = 2
let mod: Int64 = 1000000007
while i <= n {
    let c = (a + b) % mod
    a = b
    b = c
    i += 1
}
print(b)
"""
    },
    {
        "id": 5,
        "name": "Newton-Raphson Integer Sqrt (500,000 ops)",
        "arc": """
let sum_sqrt = 0;
let i = 1;
let n = 1;
let x = 1;
let y = 1;

fn main() {
    while (i <= 500000) {
        n = i;
        x = n;
        y = (x + 1) / 2;
        while (y < x) {
            x = y;
            y = (x + (n / x)) / 2;
        }
        sum_sqrt = sum_sqrt + x;
        i = i + 1;
    }
    println(sum_sqrt);
}
""",
        "py": """
sum_sqrt = 0
i = 1
while i <= 500000:
    n = i
    x = n
    y = (x + 1) // 2
    while y < x:
        x = y
        y = (x + (n // x)) // 2
    sum_sqrt += x
    i += 1
print(sum_sqrt)
""",
        "swift": """
var sum_sqrt: Int64 = 0
var i: Int64 = 1
while i <= 500000 {
    let n = i
    var x = n
    var y = (x + 1) / 2
    while y < x {
        x = y
        y = (x + (n / x)) / 2
    }
    sum_sqrt += x
    i += 1
}
print(sum_sqrt)
"""
    },
    {
        "id": 6,
        "name": "Euclidean GCD Accumulator (200,000 pairs)",
        "arc": """
let total_gcd = 0;
let i = 1;
let a = 1;
let b = 2520;
let t = 0;

fn main() {
    while (i <= 200000) {
        a = i;
        b = 2520;
        while (b != 0) {
            t = b;
            b = a % b;
            a = t;
        }
        total_gcd = total_gcd + a;
        i = i + 1;
    }
    println(total_gcd);
}
""",
        "py": """
total_gcd = 0
i = 1
while i <= 200000:
    a = i
    b = 2520
    while b != 0:
        t = b
        b = a % b
        a = t
    total_gcd += a
    i += 1
print(total_gcd)
""",
        "swift": """
var total_gcd: Int64 = 0
var i: Int64 = 1
while i <= 200000 {
    var a = i
    var b: Int64 = 2520
    while b != 0 {
        let t = b
        b = a % b
        a = t
    }
    total_gcd += a
    i += 1
}
print(total_gcd)
"""
    },
    {
        "id": 7,
        "name": "Aliquot Divisor Sum (1..8,000)",
        "arc": """
let grand_sum = 0;
let i = 1;
let d = 1;
let cur_sum = 0;

fn main() {
    while (i <= 8000) {
        cur_sum = 0;
        d = 1;
        while (d < i) {
            if (i % d == 0) {
                cur_sum = cur_sum + d;
            }
            d = d + 1;
        }
        grand_sum = grand_sum + cur_sum;
        i = i + 1;
    }
    println(grand_sum);
}
""",
        "py": """
grand_sum = 0
i = 1
while i <= 8000:
    cur_sum = 0
    d = 1
    while d < i:
        if i % d == 0:
            cur_sum += d
        d += 1
    grand_sum += cur_sum
    i += 1
print(grand_sum)
""",
        "swift": """
var grand_sum: Int64 = 0
var i: Int64 = 1
while i <= 8000 {
    var cur_sum: Int64 = 0
    var d: Int64 = 1
    while d < i {
        if i % d == 0 {
            cur_sum += d
        }
        d += 1
    }
    grand_sum += cur_sum
    i += 1
}
print(grand_sum)
"""
    },
    {
        "id": 8,
        "name": "Linear Congruential PRNG (2,000,000 ops)",
        "arc": """
let seed = 123456789;
let a = 1103515245;
let c = 12345;
let m = 2147483647;
let sum_rand = 0;
let i = 0;

fn main() {
    while (i < 2000000) {
        seed = ((seed * a) + c) % m;
        if (seed < 0) {
            seed = seed + m;
        }
        sum_rand = (sum_rand + (seed % 100)) % 1000000007;
        i = i + 1;
    }
    println(sum_rand);
}
""",
        "py": """
seed = 123456789
a = 1103515245
c = 12345
m = 2147483647
sum_rand = 0
i = 0
while i < 2000000:
    seed = (seed * a + c) % m
    if seed < 0:
        seed += m
    sum_rand = (sum_rand + (seed % 100)) % 1000000007
    i += 1
print(sum_rand)
""",
        "swift": """
var seed: Int64 = 123456789
let a: Int64 = 1103515245
let c: Int64 = 12345
let m: Int64 = 2147483647
var sum_rand: Int64 = 0
var i = 0
while i < 2000000 {
    seed = (seed * a + c) % m
    if seed < 0 {
        seed += m
    }
    sum_rand = (sum_rand + (seed % 100)) % 1000000007
    i += 1
}
print(sum_rand)
"""
    },
    {
        "id": 9,
        "name": "Sum of Polynomial Powers (5,000,000 ops)",
        "arc": """
let mod = 1000000007;
let total = 0;
let i = 1;
let sq = 0;
let cu = 0;

fn main() {
    while (i <= 5000000) {
        sq = (i * i) % mod;
        cu = (sq * i) % mod;
        total = (total + sq + cu) % mod;
        i = i + 1;
    }
    println(total);
}
""",
        "py": """
mod = 1000000007
total = 0
i = 1
while i <= 5000000:
    sq = (i * i) % mod
    cu = (sq * i) % mod
    total = (total + sq + cu) % mod
    i += 1
print(total)
""",
        "swift": """
let mod: Int64 = 1000000007
var total: Int64 = 0
var i: Int64 = 1
while i <= 5000000 {
    let sq = (i * i) % mod
    let cu = (sq * i) % mod
    total = (total + sq + cu) % mod
    i += 1
}
print(total)
"""
    },
    {
        "id": 10,
        "name": "Nested Combinatorics Triangle (N=1,500)",
        "arc": """
let n = 1500;
let mod = 1000000007;
let grand_total = 0;
let i = 1;
let j = 1;

fn main() {
    while (i <= n) {
        j = 1;
        while (j <= i) {
            grand_total = (grand_total + (i * j)) % mod;
            j = j + 1;
        }
        i = i + 1;
    }
    println(grand_total);
}
""",
        "py": """
n = 1500
mod = 1000000007
grand_total = 0
i = 1
while i <= n:
    j = 1
    while j <= i:
        grand_total = (grand_total + (i * j)) % mod
        j += 1
    i += 1
print(grand_total)
""",
        "swift": """
let n = 1500
let mod: Int64 = 1000000007
var grand_total: Int64 = 0
var i = 1
while i <= n {
    var j = 1
    while j <= i {
        grand_total = (grand_total + Int64(i * j)) % mod
        j += 1
    }
    i += 1
}
print(grand_total)
"""
    }
]

print("==========================================================================================")
print("  REAL HARDWARE BENCHMARK: ARDIUM (-O NATIVE) vs PYTHON 3 vs APPLE SWIFT (-O NATIVE)")
print("  Host: MacBook Air (Apple M2, 16GB) | macOS Darwin arm64 | 100% Real Execution")
print("==========================================================================================")

results = []

for b in BENCHMARKS:
    bid = b["id"]
    name = b["name"]
    print(f"\n[{bid}/10] Testing: {name}...", flush=True)

    arc_file = f"benchmarks/bench_{bid}.ar"
    arc_bin = f"benchmarks/bench_{bid}_arc_native"
    py_file = f"benchmarks/bench_{bid}.py"
    swift_file = f"benchmarks/bench_{bid}.swift"
    swift_bin = f"benchmarks/bench_{bid}_swift"

    with open(arc_file, "w") as f: f.write(b["arc"])
    with open(py_file, "w") as f: f.write(b["py"])
    with open(swift_file, "w") as f: f.write(b["swift"])

    # 1. Compile Native Arc (-O)
    subprocess.run([ARC_BIN, "build", arc_file, "-O", "-o", arc_bin], check=True)

    # 2. Compile Native Swift (-O)
    subprocess.run(["swiftc", "-O", swift_file, "-o", swift_bin], check=True)

    # 3. Benchmark Ardium Native (-O)
    arc_times = []
    arc_out = ""
    for _ in range(5):
        t0 = time.perf_counter()
        res = subprocess.run([f"./{arc_bin}"], capture_output=True, text=True)
        t1 = time.perf_counter()
        arc_times.append((t1 - t0) * 1000)
        arc_out = res.stdout.strip()
    arc_avg = statistics.mean(arc_times)

    # 4. Benchmark Python 3
    py_times = []
    py_out = ""
    for _ in range(5):
        t0 = time.perf_counter()
        res = subprocess.run(["python3", py_file], capture_output=True, text=True)
        t1 = time.perf_counter()
        py_times.append((t1 - t0) * 1000)
        py_out = res.stdout.strip()
    py_avg = statistics.mean(py_times)

    # 5. Benchmark Apple Swift Native (-O)
    swift_times = []
    swift_out = ""
    for _ in range(5):
        t0 = time.perf_counter()
        res = subprocess.run([f"./{swift_bin}"], capture_output=True, text=True)
        t1 = time.perf_counter()
        swift_times.append((t1 - t0) * 1000)
        swift_out = res.stdout.strip()
    swift_avg = statistics.mean(swift_times)

    match = (arc_out == py_out == swift_out)
    status_str = "✅ PASS" if match else f"❌ MISMATCH (Arc:{arc_out}, Py:{py_out}, Swift:{swift_out})"

    results.append({
        "id": bid,
        "name": name,
        "arc_ms": arc_avg,
        "py_ms": py_avg,
        "swift_ms": swift_avg,
        "output": arc_out,
        "valid": match
    })

    print(f"   -> Result: {arc_out} [{status_str}]", flush=True)
    print(f"      Ardium (-O) : {arc_avg:.2f} ms", flush=True)
    print(f"      Swift (-O)  : {swift_avg:.2f} ms", flush=True)
    print(f"      Python 3.11 : {py_avg:.2f} ms ({(py_avg/arc_avg if arc_avg else 0):.1f}x slower than Arc)", flush=True)

print("\n\n==========================================================================================")
print("                                 FINAL BENCHMARK SUMMARY TABLE")
print("==========================================================================================")
print(f"{'#':<3} | {'Benchmark Name':<42} | {'Output':<12} | {'Ardium (-O)':<12} | {'Swift (-O)':<12} | {'Python 3':<12} | {'Status'}")
print("-" * 110)
for r in results:
    print(f"{r['id']:<3} | {r['name']:<42} | {r['output']:<12} | {r['arc_ms']:>8.2f} ms | {r['swift_ms']:>8.2f} ms | {r['py_ms']:>8.2f} ms | {'✅ Valid' if r['valid'] else '❌ Fail'}")
print("==========================================================================================")
