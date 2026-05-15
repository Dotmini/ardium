// Topic: Concurrency (Goroutines)
// Run a task in background

package main

import (
	"fmt"
	"sync"
	"time"
)

func worker(id int, wg *sync.WaitGroup) {
	defer wg.Done()
	fmt.Printf("Worker running... ID: %d\n", id)
	time.Sleep(1 * time.Second)
	fmt.Println("Worker done.")
}

func main() {
	var wg sync.WaitGroup

	fmt.Println("Main: Starting worker")
	wg.Add(1)
	go worker(1, &wg)

	fmt.Println("Main: Waiting for worker")
	wg.Wait()

	fmt.Println("Main: Done")
}
