// Topic: Arrays (Slices)
// Sum elements of an array

package main

import "fmt"

func main() {
	size := 5
	arr := make([]int, size)

	// Fill
	for i := 0; i < size; i++ {
		arr[i] = i * 10
	}

	// Sum
	sum := 0
	for _, val := range arr {
		sum += val
	}

	fmt.Printf("Sum: %d\n", sum)
}
