
total_steps = 0
i = 1
while i <= 20000:
    n = i
    steps = 0
    while n != 1:
        if n % 2 == 0:
            n = n // 2
        else:
            n = 3 * n + 1
        steps += 1
    total_steps += steps
    i += 1
print(total_steps)
