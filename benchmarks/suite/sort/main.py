import sys


def main():
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 5000000
    xs = [0] * n
    x = 42
    for i in range(n):
        x = (x * 1103515245 + 12345) % 2147483648
        xs[i] = x % 1000000000
    xs.sort()
    checksum = 0
    for i in range(n):
        checksum = (checksum * 31 + xs[i]) % 1000000007
    print(xs[0], xs[n // 2], xs[n - 1], checksum)


main()
