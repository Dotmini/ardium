// Topic: Arrays (Vec)
// Sum elements of an array

fn main() {
    let size = 5;
    let mut arr = Vec::with_capacity(size);

    // Fill
    for i in 0..size {
        arr.push(i * 10);
    }

    // Sum
    let mut sum = 0;
    for val in &arr {
        sum += val;
    }

    println!("Sum: {}", sum);
}
