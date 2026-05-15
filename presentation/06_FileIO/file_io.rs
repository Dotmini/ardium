// Topic: File I/O
// Write to test.txt

use std::fs::File;
use std::io::prelude::*;

fn main() -> std::io::Result<()> {
    let mut file = File::create("test.txt")?;
    file.write_all(b"Hello Rust File IO")?;
    println!("Written to test.txt");
    Ok(())
}
