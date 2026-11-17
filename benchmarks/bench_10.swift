
let n = 1500
let mod: Int64 = 1000000007
var grand_total: Int64 = 0
var i = 1
while i <= n {
    var j = 1
    while j <= i {
        grand_total = (grand_total + Int64(i * j)) % mod
        j += 1
    }
    i += 1
}
print(grand_total)
