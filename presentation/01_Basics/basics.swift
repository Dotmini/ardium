// Topic: Basic Control Flow
// Count 0-9 and identify Even/Odd

import Foundation

var i = 0
while i < 10 {
    if i % 2 == 0 {
        print("Count: \(i) (Even)")
    } else {
        print("Count: \(i) (Odd)")
    }
    i += 1
}
