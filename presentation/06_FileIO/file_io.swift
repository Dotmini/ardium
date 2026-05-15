// Topic: File I/O
// Write to test.txt

import Foundation

let str = "Hello Swift File IO"
let filename = "test.txt"

do {
    try str.write(toFile: filename, atomically: true, encoding: .utf8)
    print("Written to test.txt")
} catch {
    print("Failed to open file")
}
