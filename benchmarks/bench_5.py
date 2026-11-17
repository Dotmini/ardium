
sum_sqrt = 0
i = 1
while i <= 500000:
    n = i
    x = n
    y = (x + 1) // 2
    while y < x:
        x = y
        y = (x + (n // x)) // 2
    sum_sqrt += x
    i += 1
print(sum_sqrt)
