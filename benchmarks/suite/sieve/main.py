import sys


def main():
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 50000000
    sieve = [True] * (n + 1)
    sieve[0] = False
    sieve[1] = False
    i = 2
    while i * i <= n:
        if sieve[i]:
            j = i * i
            while j <= n:
                sieve[j] = False
                j += i
        i += 1
    count = 0
    for k in range(n + 1):
        if sieve[k]:
            count += 1
    print(count)


main()
