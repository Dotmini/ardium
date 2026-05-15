// Topic: Advanced Global Scope
// Manage Global State (Singleton / Global Var)

import Foundation

struct AppState {
    static var status = "Active"
    static var users = 0
}

print("Current Status: \(AppState.status)")

AppState.status = "Maintenance"

print("New Status: \(AppState.status)")
