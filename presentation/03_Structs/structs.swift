// Topic: Structs
// Define Vector2 and Dot Product

struct Vector2 {
    var x: Int
    var y: Int
}

func dot(_ a: Vector2, _ b: Vector2) -> Int {
    return (a.x * b.x) + (a.y * b.y)
}

let v1 = Vector2(x: 10, y: 20)
let v2 = Vector2(x: 5, y: 5)

let result = dot(v1, v2)
print("Dot Product: \(result)")
