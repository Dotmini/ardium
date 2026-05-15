// Benchmark: Sieve of Eratosthenes

use std::time::Instant;

fn run_sieve(n: usize) -> u32 {
    let mut count = 0;
    // Using bool vector.
    // Optimizations: Rust Vec<bool> uses 1 byte per bool.
    let mut flags = vec![true; n];
    
    let mut i = 2;
    while i * i < n {
        if flags[i] {
            let mut j = i * i;
            while j < n {
                flags[j] = false;
                j += i;
            }
        }
        i += 1;
    }
    
    for i in 2..n {
        if flags[i] {
            count += 1;
        }
    }
    count
}

fn main() {
    let n = 1000000;
    println!("Finding primes up to {}...", n);
    let start = Instant::now();
    let c = run_sieve(n);
    let duration = start.elapsed();
    println!("Found: {}", c);
    println!("Time: {:?}", duration);
}
