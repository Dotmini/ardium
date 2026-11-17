package main

import (
	"fmt"
	"os"
	"strconv"
)

func main() {
	n := 400
	if len(os.Args) > 1 {
		if v, err := strconv.Atoi(os.Args[1]); err == nil {
			n = v
		}
	}
	a := make([][]float64, 0, n)
	b := make([][]float64, 0, n)
	for i := 0; i < n; i++ {
		ra := make([]float64, n)
		rb := make([]float64, n)
		for j := 0; j < n; j++ {
			ra[j] = float64((i*j)%7 + 1)
			rb[j] = float64((i+j)%5 + 1)
		}
		a = append(a, ra)
		b = append(b, rb)
	}
	c := make([][]float64, 0, n)
	for i := 0; i < n; i++ {
		row := make([]float64, n)
		ai := a[i]
		for k := 0; k < n; k++ {
			aik := ai[k]
			bk := b[k]
			for j := 0; j < n; j++ {
				row[j] += aik * bk[j]
			}
		}
		c = append(c, row)
	}
	total := 0.0
	for i := 0; i < n; i++ {
		ci := c[i]
		for j := 0; j < n; j++ {
			total += ci[j]
		}
	}
	fmt.Println(int64(total), int64(c[n/2][n/2]))
}
