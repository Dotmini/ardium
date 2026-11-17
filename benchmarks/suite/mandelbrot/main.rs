fn main() {
    let size: usize = std::env::args().nth(1).and_then(|s| s.parse().ok()).unwrap_or(2000);
    let max_iter = 100;
    let mut inside: i64 = 0;
    for y in 0..size {
        let ci = -1.5 + 3.0 * y as f64 / size as f64;
        for x in 0..size {
            let cr = -2.0 + 3.0 * x as f64 / size as f64;
            let mut zr = 0.0f64;
            let mut zi = 0.0f64;
            let mut i = 0;
            let mut escaped = false;
            while i < max_iter {
                let zr2 = zr * zr;
                let zi2 = zi * zi;
                if zr2 + zi2 > 4.0 {
                    escaped = true;
                    break;
                }
                zi = 2.0 * zr * zi + ci;
                zr = zr2 - zi2 + cr;
                i += 1;
            }
            if !escaped {
                inside += 1;
            }
        }
    }
    println!("{}", inside);
}
