
grand_sum = 0
i = 1
while i <= 8000:
    cur_sum = 0
    d = 1
    while d < i:
        if i % d == 0:
            cur_sum += d
        d += 1
    grand_sum += cur_sum
    i += 1
print(grand_sum)
