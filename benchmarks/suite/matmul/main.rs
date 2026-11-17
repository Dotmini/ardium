fn main() {
    let n: usize = std::env::args().nth(1).and_then(|s| s.parse().ok()).unwrap_or(400);
    let mut a: Vec<Vec<f64>> = Vec::new();
    let mut b: Vec<Vec<f64>> = Vec::new();
    for i in 0..n {
        let mut ra = vec![0.0; n];
        let mut rb = vec![0.0; n];
        for j in 0..n {
            ra[j] = ((i * j) % 7 + 1) as f64;
            rb[j] = ((i + j) % 5 + 1) as f64;
        }
        a.push(ra);
        b.push(rb);
    }
    let mut c: Vec<Vec<f64>> = Vec::new();
    for i in 0..n {
        let mut row = vec![0.0; n];
        let ai = &a[i];
        for k in 0..n {
            let aik = ai[k];
            let bk = &b[k];
            for j in 0..n {
                row[j] += aik * bk[j];
            }
        }
        c.push(row);
    }
    let mut total = 0.0;
    for i in 0..n {
        let ci = &c[i];
        for j in 0..n {
            total += ci[j];
        }
    }
    println!("{} {}", total as i64, c[n / 2][n / 2] as i64);
}
