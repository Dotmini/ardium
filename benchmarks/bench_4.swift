
let n = 2000000
var a: Int64 = 0
var b: Int64 = 1
var i = 2
let mod: Int64 = 1000000007
while i <= n {
    let c = (a + b) % mod
    a = b
    b = c
    i += 1
}
print(b)
