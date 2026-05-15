// Topic: Structs
// Define Vector2 and Dot Product

struct Vector2 {
    x: i32,
    y: i32,
}

fn dot(a: &Vector2, b: &Vector2) -> i32 {
    return (a.x * b.x) + (a.y * b.y);
}

fn main() {
    let v1 = Vector2 { x: 10, y: 20 };
    let v2 = Vector2 { x: 5, y: 5 };

    let result = dot(&v1, &v2);
    println!("Dot Product: {}", result);
}
