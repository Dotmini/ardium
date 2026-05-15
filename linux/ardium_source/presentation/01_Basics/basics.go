// Topic: Basic Control Flow
// Count 0-9 and identify Even/Odd

package main

import "fmt"

func main() {
    i := 0
    for i < 10 {
        if i % 2 == 0 {
            fmt.Printf("Count: %d (Even)\n", i)
        } else {
            fmt.Printf("Count: %d (Odd)\n", i)
        }
        i++
    }
}
