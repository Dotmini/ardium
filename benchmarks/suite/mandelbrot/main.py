import sys


def main():
    size = int(sys.argv[1]) if len(sys.argv) > 1 else 2000
    max_iter = 100
    inside = 0
    for y in range(size):
        ci = -1.5 + 3.0 * y / size
        for x in range(size):
            cr = -2.0 + 3.0 * x / size
            zr = 0.0
            zi = 0.0
            escaped = False
            for _ in range(max_iter):
                zr2 = zr * zr
                zi2 = zi * zi
                if zr2 + zi2 > 4.0:
                    escaped = True
                    break
                zi = 2.0 * zr * zi + ci
                zr = zr2 - zi2 + cr
            if not escaped:
                inside += 1
    print(inside)


main()
