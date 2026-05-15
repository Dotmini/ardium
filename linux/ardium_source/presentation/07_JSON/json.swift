// Topic: JSON Handling
// Parse JSON string

import Foundation

struct Config: Codable {
    let name: String
    let vers: Int
}

let jsonStr = "{\"name\": \"Ardium\", \"vers\": 2}"
let jsonData = jsonStr.data(using: .utf8)!

let decoder = JSONDecoder()
if let config = try? decoder.decode(Config.self, from: jsonData) {
    print("Name: \(config.name)")
    print("Version: \(config.vers)")
}
