// Topic: JSON Handling
// Parse JSON string

package main

import (
	"encoding/json"
	"fmt"
)

type Config struct {
	Name string `json:"name"`
	Vers int    `json:"vers"`
}

func main() {
	jsonStr := `{"name": "Ardium", "vers": 2}`
	var config Config

	json.Unmarshal([]byte(jsonStr), &config)

	fmt.Printf("Name: %s\n", config.Name)
	fmt.Printf("Version: %d\n", config.Vers)
}
