package main

import (
	"fmt"
	"os"
	"strconv"
)

func main() {
	n := 3000000
	if len(os.Args) > 1 {
		if v, err := strconv.Atoi(os.Args[1]); err == nil {
			n = v
		}
	}
	counts := map[string]int{}
	x := int64(12345)
	for i := 0; i < n; i++ {
		x = (x*1103515245 + 12345) % 2147483648
		key := "w" + strconv.FormatInt(x%50000, 10)
		counts[key]++
	}
	best := 0
	bestKey := ""
	for k, v := range counts {
		if v > best || (v == best && k < bestKey) {
			best = v
			bestKey = k
		}
	}
	fmt.Println(len(counts), bestKey, best)
}
