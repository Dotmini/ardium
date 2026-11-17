package main

import (
	"fmt"
	"os"
	"strconv"
)

func main() {
	n := 50000000
	if len(os.Args) > 1 {
		if v, err := strconv.Atoi(os.Args[1]); err == nil {
			n = v
		}
	}
	sieve := make([]bool, n+1)
	for i := range sieve {
		sieve[i] = true
	}
	sieve[0] = false
	sieve[1] = false
	for i := 2; i*i <= n; i++ {
		if sieve[i] {
			for j := i * i; j <= n; j += i {
				sieve[j] = false
			}
		}
	}
	count := 0
	for k := 0; k <= n; k++ {
		if sieve[k] {
			count++
		}
	}
	fmt.Println(count)
}
