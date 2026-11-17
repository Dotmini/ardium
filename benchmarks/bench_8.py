
seed = 123456789
a = 1103515245
c = 12345
m = 2147483647
sum_rand = 0
i = 0
while i < 2000000:
    seed = (seed * a + c) % m
    if seed < 0:
        seed += m
    sum_rand = (sum_rand + (seed % 100)) % 1000000007
    i += 1
print(sum_rand)
