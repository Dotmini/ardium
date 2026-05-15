// Benchmark: Sieve of Eratosthenes

package main

import (
	"fmt"
	"time"
)

func run_sieve(n int) int {
	count := 0
	flags := make([]bool, n)
	for i := range flags {
		flags[i] = true
	}

	for i := 2; i*i < n; i++ {
		if flags[i] {
			for j := i * i; j < n; j += i {
				flags[j] = false
			}
		}
	}

	for i := 2; i < n; i++ {
		if flags[i] {
			count++
		}
	}
	return count
}

func main() {
	n := 100000000
	fmt.Printf("Finding primes up to %d...\n", n)
	start := time.Now()
	c := run_sieve(n)
	elapsed := time.Since(start)
	fmt.Printf("Found: %d\n", c)
	fmt.Printf("Time: %s\n", elapsed)
}
