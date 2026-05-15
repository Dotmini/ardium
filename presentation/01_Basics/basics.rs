// Topic: Basic Control Flow
// Count 0-9 and identify Even/Odd

fn main() {
    let mut i = 0;
    while i < 10 {
        if i % 2 == 0 {
            println!("Count: {} (Even)", i);
        } else {
            println!("Count: {} (Odd)", i);
        }
        i += 1;
    }
}
