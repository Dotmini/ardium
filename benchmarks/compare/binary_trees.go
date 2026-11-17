package main

import (
	"fmt"
	"os"
	"strconv"
)

type Tree struct{ left, right *Tree }

func make_(depth int) *Tree {
	if depth == 0 {
		return &Tree{}
	}
	return &Tree{make_(depth - 1), make_(depth - 1)}
}

func check(t *Tree) int {
	if t.left != nil && t.right != nil {
		return 1 + check(t.left) + check(t.right)
	}
	return 1
}

func main() {
	maxDepth := 18
	if len(os.Args) > 1 {
		if v, err := strconv.Atoi(os.Args[1]); err == nil {
			maxDepth = v
		}
	}
	minDepth := 4
	stretch := maxDepth + 1
	fmt.Printf("stretch tree of depth %d\t check: %d\n", stretch, check(make_(stretch)))
	longLived := make_(maxDepth)
	for d := minDepth; d <= maxDepth; d += 2 {
		iterations := 1 << (maxDepth - d + minDepth)
		total := 0
		for i := 0; i < iterations; i++ {
			total += check(make_(d))
		}
		fmt.Printf("%d\t trees of depth %d\t check: %d\n", iterations, d, total)
	}
	fmt.Printf("long lived tree of depth %d\t check: %d\n", maxDepth, check(longLived))
}
