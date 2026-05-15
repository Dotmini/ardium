// Topic: GUI (SwiftUI)
// Create a Window with a Button

import SwiftUI

struct ContentView: View {
    var body: some View {
        VStack {
            Text("Hello Swift GUI")
            Button("Click Me!") {
                print("Button Clicked!")
            }
        }
    }
}
// Logic usually wrapped in App struct
