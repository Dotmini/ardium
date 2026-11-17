
var total_gcd: Int64 = 0
var i: Int64 = 1
while i <= 200000 {
    var a = i
    var b: Int64 = 2520
    while b != 0 {
        let t = b
        b = a % b
        a = t
    }
    total_gcd += a
    i += 1
}
print(total_gcd)
