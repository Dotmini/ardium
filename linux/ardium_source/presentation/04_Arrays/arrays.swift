// Topic: Arrays
// Sum elements of an array

import Foundation

let size = 5
var arr: [Int] = []

// Fill
for i in 0..<size {
    arr.append(i * 10)
}

// Sum
var sum = 0
for val in arr {
    sum += val
}

print("Sum: \(sum)")
