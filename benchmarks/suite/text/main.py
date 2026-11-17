import sys

CITIES = ["Bangkok", "ChiangMai", "Phuket", "KhonKaen", "HatYai", "Pattaya", "UdonThani", "Korat", "HuaHin", "Ayutthaya"]


def main():
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 1000000
    parts = []
    x = 7
    for _ in range(n):
        x = (x * 1103515245 + 12345) % 2147483648
        city = CITIES[(x // 1000) % 10]
        parts.append(f"user{x % 1000},{city},{(x // 10) % 10000}")
    data = "\n".join(parts)
    totals = {}
    users = {}
    grand = 0
    lines = data.split("\n")
    for line in lines:
        f = line.split(",")
        amount = int(f[2])
        totals[f[1]] = totals.get(f[1], 0) + amount
        users[f[0]] = users.get(f[0], 0) + 1
        grand += amount
    best = ""
    best_total = -1
    for city, t in totals.items():
        if t > best_total or (t == best_total and city < best):
            best = city
            best_total = t
    print(len(lines), len(users), best, best_total, grand)


main()
