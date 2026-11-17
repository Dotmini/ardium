import sys


def main():
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 400
    a = []
    b = []
    for i in range(n):
        ra = [0.0] * n
        rb = [0.0] * n
        for j in range(n):
            ra[j] = float((i * j) % 7 + 1)
            rb[j] = float((i + j) % 5 + 1)
        a.append(ra)
        b.append(rb)
    c = []
    for i in range(n):
        row = [0.0] * n
        ai = a[i]
        for k in range(n):
            aik = ai[k]
            bk = b[k]
            for j in range(n):
                row[j] += aik * bk[j]
        c.append(row)
    total = 0.0
    for i in range(n):
        ci = c[i]
        for j in range(n):
            total += ci[j]
    print(int(total), int(c[n // 2][n // 2]))


main()
