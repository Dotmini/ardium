// Topic: Advanced Global Scope
// Manage Global State

package main

import "fmt"

var AppState = struct {
	Status string
	Users  int
}{
	Status: "Active",
	Users:  0,
}

func main() {
	fmt.Printf("Current Status: %s\n", AppState.Status)

	AppState.Status = "Maintenance"

	fmt.Printf("New Status: %s\n", AppState.Status)
}
