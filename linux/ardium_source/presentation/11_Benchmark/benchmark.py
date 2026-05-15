# Benchmark: Sieve of Eratosthenes

import time

def run_sieve(n):
    count = 0
    flags = [True] * n
    
    i = 2
    while i * i < n:
        if flags[i]:
            j = i * i
            while j < n:
                flags[j] = False
                j += i
        i += 1
    
    i = 2
    while i < n:
        if flags[i]:
            count += 1
        i += 1
    
    return count

def main():
    n = 100000000
    print(f"Finding primes up to {n}...")
    start = time.time()
    c = run_sieve(n)
    end = time.time()
    print(f"Found: {c}")
    print(f"Time: {(end - start):.4f}s")

if __name__ == "__main__":
    main()
