
var sum_sqrt: Int64 = 0
var i: Int64 = 1
while i <= 500000 {
    let n = i
    var x = n
    var y = (x + 1) / 2
    while y < x {
        x = y
        y = (x + (n / x)) / 2
    }
    sum_sqrt += x
    i += 1
}
print(sum_sqrt)
