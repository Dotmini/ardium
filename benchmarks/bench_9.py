
mod = 1000000007
total = 0
i = 1
while i <= 5000000:
    sq = (i * i) % mod
    cu = (sq * i) % mod
    total = (total + sq + cu) % mod
    i += 1
print(total)
