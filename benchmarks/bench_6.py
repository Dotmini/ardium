
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
