
var grand_sum: Int64 = 0
var i: Int64 = 1
while i <= 8000 {
    var cur_sum: Int64 = 0
    var d: Int64 = 1
    while d < i {
        if i % d == 0 {
            cur_sum += d
        }
        d += 1
    }
    grand_sum += cur_sum
    i += 1
}
print(grand_sum)
