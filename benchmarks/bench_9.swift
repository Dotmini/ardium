
let mod: Int64 = 1000000007
var total: Int64 = 0
var i: Int64 = 1
while i <= 5000000 {
    let sq = (i * i) % mod
    let cu = (sq * i) % mod
    total = (total + sq + cu) % mod
    i += 1
}
print(total)
