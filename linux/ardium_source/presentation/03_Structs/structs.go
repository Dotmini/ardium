// Topic: Structs
// Define Vector2 and Dot Product

package main

import "fmt"

type Vector2 struct {
    x int
    y int
}

func dot(a Vector2, b Vector2) int {
    return (a.x * b.x) + (a.y * b.y)
}

func main() {
    v1 := Vector2{x: 10, y: 20}
    v2 := Vector2{x: 5, y: 5}

    result := dot(v1, v2)
    fmt.Printf("Dot Product: %d\n", result)
}
