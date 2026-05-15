// Topic: JSON Handling (Mock)
// Parse: {"name": "Ardium", "vers": 2}

struct Config {
    name: String,
    vers: i32,
}

fn main() {
    let json = "{\"name\": \"Ardium\", \"vers\": 2}";
    
    // Mock parsing for demo without external dependencies
    // In real Rust, use serde_json
    
    let name = "Ardium";
    let vers = 2;
    
    println!("Name: {}", name);
    println!("Version: {}", vers);
}
