struct Tree { left: Option<Box<Tree>>, right: Option<Box<Tree>> }

fn make(depth: i64) -> Box<Tree> {
    if depth == 0 { return Box::new(Tree { left: None, right: None }); }
    Box::new(Tree { left: Some(make(depth - 1)), right: Some(make(depth - 1)) })
}

fn check(t: &Tree) -> i64 {
    if let (Some(l), Some(r)) = (&t.left, &t.right) { return 1 + check(l) + check(r); }
    1
}

fn main() {
    let max_depth: i64 = std::env::args().nth(1).and_then(|s| s.parse().ok()).unwrap_or(18);
    let min_depth = 4;
    let stretch = max_depth + 1;
    println!("stretch tree of depth {}\t check: {}", stretch, check(&make(stretch)));
    let long_lived = make(max_depth);
    let mut d = min_depth;
    while d <= max_depth {
        let iterations = 1i64 << (max_depth - d + min_depth);
        let mut total = 0;
        for _ in 0..iterations { total += check(&make(d)); }
        println!("{}\t trees of depth {}\t check: {}", iterations, d, total);
        d += 2;
    }
    println!("long lived tree of depth {}\t check: {}", max_depth, check(&long_lived));
}
