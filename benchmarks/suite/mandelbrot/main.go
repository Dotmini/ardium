package main

import (
	"fmt"
	"os"
	"strconv"
)

func main() {
	size := 2000
	if len(os.Args) > 1 {
		if v, err := strconv.Atoi(os.Args[1]); err == nil {
			size = v
		}
	}
	maxIter := 100
	inside := 0
	for y := 0; y < size; y++ {
		ci := -1.5 + 3.0*float64(y)/float64(size)
		for x := 0; x < size; x++ {
			cr := -2.0 + 3.0*float64(x)/float64(size)
			zr, zi := 0.0, 0.0
			escaped := false
			for i := 0; i < maxIter; i++ {
				zr2 := zr * zr
				zi2 := zi * zi
				if zr2+zi2 > 4.0 {
					escaped = true
					break
				}
				// float64(...) rounds the product, so Go does not fuse it into
				// a multiply-add: results stay IEEE-identical to the other languages
				zi = float64(2.0*zr*zi) + ci
				zr = zr2 - zi2 + cr
			}
			if !escaped {
				inside++
			}
		}
	}
	fmt.Println(inside)
}
