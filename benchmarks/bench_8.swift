
var seed: Int64 = 123456789
let a: Int64 = 1103515245
let c: Int64 = 12345
let m: Int64 = 2147483647
var sum_rand: Int64 = 0
var i = 0
while i < 2000000 {
    seed = (seed * a + c) % m
    if seed < 0 {
        seed += m
    }
    sum_rand = (sum_rand + (seed % 100)) % 1000000007
    i += 1
}
print(sum_rand)
