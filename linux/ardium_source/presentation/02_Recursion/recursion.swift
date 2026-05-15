// Topic: Recursion
// Calculate Fibonacci(10)

import Foundation

func fib(_ n: Int) -> Int {
    if n <= 1 {
        return n
    }
    return fib(n - 1) + fib(n - 2)
}

let result = fib(10)
print("Fibonacci(10) = \(result)")
