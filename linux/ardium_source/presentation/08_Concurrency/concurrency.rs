// Topic: Concurrency (Threads)
// Run a task in background

use std::thread;
use std::time::Duration;

fn main() {
    println!("Main: Starting worker");
    
    let handle = thread::spawn(|| {
        println!("Worker running... ID: 1");
        thread::sleep(Duration::from_secs(1));
        println!("Worker done.");
    });
    
    println!("Main: Waiting for worker");
    handle.join().unwrap();
    
    println!("Main: Done");
}
