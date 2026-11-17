import sys


def make(depth):
    if depth == 0:
        return (None, None)
    return (make(depth - 1), make(depth - 1))


def check(t):
    left, right = t
    if left is not None and right is not None:
        return 1 + check(left) + check(right)
    return 1


def main():
    max_depth = int(sys.argv[1]) if len(sys.argv) > 1 else 18
    min_depth = 4
    stretch = max_depth + 1
    print(f"stretch tree of depth {stretch}\t check: {check(make(stretch))}")
    long_lived = make(max_depth)
    for d in range(min_depth, max_depth + 1, 2):
        iterations = 1 << (max_depth - d + min_depth)
        total = 0
        for _ in range(iterations):
            total += check(make(d))
        print(f"{iterations}\t trees of depth {d}\t check: {total}")
    print(f"long lived tree of depth {max_depth}\t check: {check(long_lived)}")


main()
