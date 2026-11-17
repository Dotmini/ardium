
let base: Int64 = 7
let exp: Int64 = 256
let mod: Int64 = 1000000007
var checksum: Int64 = 0
var iter_count = 0
while iter_count < 100000 {
    var res: Int64 = 1
    var b = base
    var e = exp
    while e > 0 {
        if e % 2 == 1 {
            res = (res * b) % mod
        }
        b = (b * b) % mod
        e = e / 2
    }
    checksum = (checksum + res) % mod
    iter_count += 1
}
print(checksum)
