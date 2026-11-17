
var total_steps: Int64 = 0
var i: Int64 = 1
while i <= 20000 {
    var n = i
    var steps: Int64 = 0
    while n != 1 {
        if n % 2 == 0 {
            n = n / 2
        } else {
            n = 3 * n + 1
        }
        steps += 1
    }
    total_steps += steps
    i += 1
}
print(total_steps)
