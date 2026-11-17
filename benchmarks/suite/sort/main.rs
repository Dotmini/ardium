fn main() {
    let n: usize = std::env::args().nth(1).and_then(|s| s.parse().ok()).unwrap_or(5000000);
    let mut xs = vec![0i64; n];
    let mut x: i64 = 42;
    for i in 0..n {
        x = (x * 1103515245 + 12345) % 2147483648;
        xs[i] = x % 1000000000;
    }
    xs.sort_unstable();
    let mut checksum: i64 = 0;
    for i in 0..n {
        checksum = (checksum * 31 + xs[i]) % 1000000007;
    }
    println!("{} {} {} {}", xs[0], xs[n / 2], xs[n - 1], checksum);
}
