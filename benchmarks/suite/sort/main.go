package main

import (
	"fmt"
	"os"
	"slices"
	"strconv"
)

func main() {
	n := 5000000
	if len(os.Args) > 1 {
		if v, err := strconv.Atoi(os.Args[1]); err == nil {
			n = v
		}
	}
	xs := make([]int64, n)
	x := int64(42)
	for i := 0; i < n; i++ {
		x = (x*1103515245 + 12345) % 2147483648
		xs[i] = x % 1000000000
	}
	slices.Sort(xs)
	checksum := int64(0)
	for i := 0; i < n; i++ {
		checksum = (checksum*31 + xs[i]) % 1000000007
	}
	fmt.Println(xs[0], xs[n/2], xs[n-1], checksum)
}
