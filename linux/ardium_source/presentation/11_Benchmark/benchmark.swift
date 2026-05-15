// Benchmark: Sieve of Eratosthenes

import Foundation

func run_sieve(_ n: Int) -> Int {
    var count = 0
    var flags = [Bool](repeating: true, count: n)
    
    var i = 2
    while i * i < n {
        if flags[i] {
            var j = i * i
            while j < n {
                flags[j] = false
                j += i
            }
        }
        i += 1
    }
    
    for k in 2..<n {
        if flags[k] {
            count += 1
        }
    }
    return count
}

let n = 100000000
print("Finding primes up to \(n)...")
let start = CFAbsoluteTimeGetCurrent()
let c = run_sieve(n)
let diff = CFAbsoluteTimeGetCurrent() - start
print("Found: \(c)")
print("Time: \(diff)s")
