import SwiftUI
import Cocoa

// Global State
class ArdiumAppState: ObservableObject {
    @Published var components: [AnyView] = []
}

// Delegate
class ArdiumDelegate: NSObject, NSApplicationDelegate {
    var window: NSWindow!
    let state = ArdiumAppState()
    var windowTitle: String = "Ardium App"

    func applicationDidFinishLaunching(_ notification: Notification) {
        let contentView = ArdiumRootView(state: state)
        
        window = NSWindow(
            contentRect: NSRect(x: 0, y: 0, width: 800, height: 600),
            styleMask: [.titled, .closable, .miniaturizable, .resizable],
            backing: .buffered, defer: false)
        window.title = windowTitle
        window.center()
        window.contentView = NSHostingView(rootView: contentView)
        window.makeKeyAndOrderFront(nil)
        NSApp.activate(ignoringOtherApps: true)
    }
}

// Root View
struct ArdiumRootView: View {
    @ObservedObject var state: ArdiumAppState
    
    var body: some View {
        VStack(spacing: 20) {
            Text("Welcome to Ardium SwiftUI")
                .font(.headline)
                .padding()
            
            ForEach(0..<state.components.count, id: \.self) { i in
                state.components[i]
            }
            
            Spacer()
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity)
        .padding()
    }
}

// Global reference
var globalDelegate: ArdiumDelegate?

// --- C Exports ---

@_cdecl("swiftCreateApp")
public func swiftCreateApp(title: UnsafePointer<CChar>) {
    let titleStr = String(cString: title)
    
    let app = NSApplication.shared
    globalDelegate = ArdiumDelegate()
    globalDelegate?.windowTitle = titleStr
    app.delegate = globalDelegate
    app.setActivationPolicy(.regular)
}

@_cdecl("swiftAddButton")
public func swiftAddButton(label: UnsafePointer<CChar>) {
    let labelStr = String(cString: label)
    
    DispatchQueue.main.async { // Ensure UI updates on main thread
        globalDelegate?.state.components.append(AnyView(
            Button(action: {
                print("Clicked: \(labelStr)")
            }) {
                Text(labelStr)
                    .padding()
                    .background(Color.blue)
                    .foregroundColor(.white)
                    .cornerRadius(8)
            }
        ))
    }
}

@_cdecl("swiftRunApp")
public func swiftRunApp() {
    NSApp.run()
}
