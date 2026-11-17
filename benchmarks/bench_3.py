
base = 7
exp = 256
mod = 1000000007
checksum = 0
iter_count = 0
while iter_count < 100000:
    res = 1
    b = base
    e = exp
    while e > 0:
        if e % 2 == 1:
            res = (res * b) % mod
        b = (b * b) % mod
        e = e // 2
    checksum = (checksum + res) % mod
    iter_count += 1
print(checksum)
