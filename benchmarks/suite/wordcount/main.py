import sys


def main():
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 3000000
    counts = {}
    x = 12345
    for _ in range(n):
        x = (x * 1103515245 + 12345) % 2147483648
        key = f"w{x % 50000}"
        counts[key] = counts.get(key, 0) + 1
    best = 0
    best_key = ""
    for k, v in counts.items():
        if v > best or (v == best and k < best_key):
            best = v
            best_key = k
    print(len(counts), best_key, best)


main()
