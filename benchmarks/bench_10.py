
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
