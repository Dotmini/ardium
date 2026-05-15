// Topic: Concurrency (Grand Central Dispatch / Async)
// Run a task in background

import Foundation

let group = DispatchGroup()
group.enter()

print("Main: Starting worker")

DispatchQueue.global().async {
    print("Worker running... ID: 1")
    sleep(1)
    print("Worker done.")
    group.leave()
}

print("Main: Waiting for worker")
group.wait()

print("Main: Done")
