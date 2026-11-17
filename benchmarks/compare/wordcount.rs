use std::collections::HashMap;

fn main() {
    let n: i64 = std::env::args().nth(1).and_then(|s| s.parse().ok()).unwrap_or(3000000);
    let mut counts: HashMap<String, i64> = HashMap::new();
    let mut x: i64 = 12345;
    for _ in 0..n {
        x = (x * 1103515245 + 12345) % 2147483648;
        let key = format!("w{}", x % 50000);
        *counts.entry(key).or_insert(0) += 1;
    }
    let mut best = 0;
    let mut best_key = String::new();
    for (k, &v) in &counts {
        if v > best || (v == best && *k < best_key) {
            best = v;
            best_key = k.clone();
        }
    }
    println!("{} {} {}", counts.len(), best_key, best);
}
