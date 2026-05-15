// Topic: Recursion
// Calculate Fibonacci(10)

package main

import "fmt"

func fib(n int) int {
    if n <= 1 {
        return n
    }
    return fib(n-1) + fib(n-2)
}

func main() {
    result := fib(10)
    fmt.Printf("Fibonacci(10) = %d\n", result)
}
