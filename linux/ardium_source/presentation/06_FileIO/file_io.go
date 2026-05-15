// Topic: File I/O
// Write to test.txt

package main

import (
	"fmt"
	"os"
)

func main() {
	d1 := []byte("Hello Go File IO")
	err := os.WriteFile("test.txt", d1, 0644)
	if err != nil {
		fmt.Println("Failed to open file")
		return
	}
	fmt.Println("Written to test.txt")
}
