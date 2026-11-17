
let limit = 15000
var count = 0
var n = 2
while n <= limit {
    var is_prime = 1
    var d = 2
    while d * d <= n {
        if n % d == 0 {
            is_prime = 0
            break
        }
        d += 1
    }
    if is_prime == 1 {
        count += 1
    }
    n += 1
}
print(count)
