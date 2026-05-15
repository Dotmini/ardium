// Topic: Strings
// Concatenation and Printing

fn main() {
    let s1 = "Hello".to_string();
    let s2 = " World";
    
    // Rust requires ownership transfer or cloning for +
    let s3 = s1 + s2;
    
    println!("Result: {}", s3);
}
