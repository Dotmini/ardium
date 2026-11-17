
n = 2000000
a = 0
b = 1
i = 2
mod = 1000000007
while i <= n:
    c = (a + b) % mod
    a = b
    b = c
    i += 1
print(b)
