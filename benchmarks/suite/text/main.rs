use std::collections::HashMap;

const CITIES: [&str; 10] = ["Bangkok", "ChiangMai", "Phuket", "KhonKaen", "HatYai", "Pattaya", "UdonThani", "Korat", "HuaHin", "Ayutthaya"];

fn main() {
    let n: usize = std::env::args().nth(1).and_then(|s| s.parse().ok()).unwrap_or(1000000);
    let mut parts: Vec<String> = Vec::new();
    let mut x: i64 = 7;
    for _ in 0..n {
        x = (x * 1103515245 + 12345) % 2147483648;
        let city = CITIES[((x / 1000) % 10) as usize];
        parts.push(format!("user{},{},{}", x % 1000, city, (x / 10) % 10000));
    }
    let data = parts.join("\n");
    let mut totals: HashMap<&str, i64> = HashMap::new();
    let mut users: HashMap<&str, i64> = HashMap::new();
    let mut grand: i64 = 0;
    let lines: Vec<&str> = data.split('\n').collect();
    for line in &lines {
        let f: Vec<&str> = line.split(',').collect();
        let amount: i64 = f[2].parse().unwrap_or(0);
        *totals.entry(f[1]).or_insert(0) += amount;
        *users.entry(f[0]).or_insert(0) += 1;
        grand += amount;
    }
    let mut best = "";
    let mut best_total: i64 = -1;
    for (&city, &t) in &totals {
        if t > best_total || (t == best_total && city < best) {
            best = city;
            best_total = t;
        }
    }
    println!("{} {} {} {} {}", lines.len(), users.len(), best, best_total, grand);
}
