import AppKit
import SwiftUI

// MARK: - Workspace

@MainActor
final class ArdiumWorkspace: ObservableObject {
    @Published var code: String = ""
    @Published var consoleOutput: String = "Ardium Studio\n>>> Ready.\n"
    @Published var isRunning: Bool = false
    @Published var statusText: String = "Ready"

    @Published var files: [String] = []
    @Published var currentFile: String? = nil

    private let projectURL: URL

    init() {
        let docs = FileManager.default.urls(for: .documentDirectory, in: .userDomainMask).first!
        self.projectURL = docs.appendingPathComponent("ArdiumProject", isDirectory: true)

        bootstrapProjectIfNeeded()
        refreshFiles()

        if files.contains("main.ar") {
            selectFile("main.ar")
        } else if let first = files.first {
            selectFile(first)
        } else {
            createFile(name: "main.ar")
            selectFile("main.ar")
        }
    }

    private func bootstrapProjectIfNeeded() {
        if !FileManager.default.fileExists(atPath: projectURL.path) {
            try? FileManager.default.createDirectory(
                at: projectURL, withIntermediateDirectories: true)
            let main = projectURL.appendingPathComponent("main.ar")
            let starter = """
                fn main() {
                    println("Hello, Ardium Studio");
                }
                """
            try? starter.write(to: main, atomically: true, encoding: .utf8)
        }
    }

    func refreshFiles() {
        let fm = FileManager.default
        let urls =
            (try? fm.contentsOfDirectory(
                at: projectURL, includingPropertiesForKeys: nil, options: [.skipsHiddenFiles]))
            ?? []
        self.files =
            urls
            .filter { ["ar", "toml", "txt"].contains($0.pathExtension.lowercased()) }
            .map { $0.lastPathComponent }
            .sorted()
    }

    func selectFile(_ filename: String) {
        let url = projectURL.appendingPathComponent(filename)
        do {
            let s = try String(contentsOf: url, encoding: .utf8)
            self.currentFile = filename
            self.code = s
            self.statusText = "Loaded \(filename)"
        } catch {
            self.consoleOutput += ">>> Error loading \(filename): \(error.localizedDescription)\n"
            self.statusText = "Error"
        }
    }

    func saveCurrentFile() {
        guard let f = currentFile else { return }
        let url = projectURL.appendingPathComponent(f)
        do {
            try code.write(to: url, atomically: true, encoding: .utf8)
            statusText = "Saved"
        } catch {
            consoleOutput += ">>> Save failed: \(error.localizedDescription)\n"
            statusText = "Error"
        }
    }

    func createFile(name: String) {
        var n = name.trimmingCharacters(in: .whitespacesAndNewlines)
        if n.isEmpty { return }
        if !n.contains(".") { n += ".ar" }

        let url = projectURL.appendingPathComponent(n)
        if FileManager.default.fileExists(atPath: url.path) {
            consoleOutput += ">>> File exists: \(n)\n"
            return
        }

        do {
            try "".write(to: url, atomically: true, encoding: .utf8)
            refreshFiles()
            selectFile(n)
            consoleOutput += ">>> Created \(n)\n"
        } catch {
            consoleOutput += ">>> Create failed: \(error.localizedDescription)\n"
        }
    }

    func deleteFile(name: String) {
        let url = projectURL.appendingPathComponent(name)
        do {
            try FileManager.default.removeItem(at: url)
            consoleOutput += ">>> Deleted \(name)\n"
            refreshFiles()
            if currentFile == name {
                currentFile = nil
                code = ""
                statusText = "Ready"
            }
        } catch {
            consoleOutput += ">>> Delete failed: \(error.localizedDescription)\n"
        }
    }

    // MARK: Run

    func run() {
        guard !isRunning else { return }
        guard let f = currentFile else { return }

        saveCurrentFile()

        isRunning = true
        statusText = "Running…"
        consoleOutput = ">>> Run \(f)\n"

        let fileURL = projectURL.appendingPathComponent(f)

        Task.detached { [weak self] in
            guard let self else { return }

            let arcCandidates = [
                "/usr/local/bin/arc",
                "/opt/homebrew/bin/arc",
                "/Users/dotmini/Documents/ardium/arc",
            ]

            let arcPath =
                arcCandidates.first(where: { FileManager.default.fileExists(atPath: $0) })
                ?? "/usr/local/bin/arc"

            let task = Process()
            task.executableURL = URL(fileURLWithPath: arcPath)
            task.arguments = ["run", fileURL.path]

            let pipe = Pipe()
            task.standardOutput = pipe
            task.standardError = pipe

            do {
                try task.run()
                task.waitUntilExit()

                let data = pipe.fileHandleForReading.readDataToEndOfFile()
                let out = String(data: data, encoding: .utf8) ?? ""

                await MainActor.run {
                    self.consoleOutput += out
                    self.consoleOutput += "\n>>> Exit \(task.terminationStatus)\n"
                    self.isRunning = false
                    self.statusText = (task.terminationStatus == 0) ? "Finished" : "Failed"
                }
            } catch {
                await MainActor.run {
                    self.consoleOutput += ">>> Execution error: \(error.localizedDescription)\n"
                    self.isRunning = false
                    self.statusText = "Error"
                }
            }
        }
    }

    func stop() {
        // Minimal UI: not holding Process ref yet.
        isRunning = false
        statusText = "Stopped"
        consoleOutput += ">>> Stop\n"
    }
}

// MARK: - Glass Materials

struct Glass: NSViewRepresentable {
    var material: NSVisualEffectView.Material
    var blendingMode: NSVisualEffectView.BlendingMode = .behindWindow

    func makeNSView(context: Context) -> NSVisualEffectView {
        let v = NSVisualEffectView()
        v.material = material
        v.blendingMode = blendingMode
        v.state = .active
        return v
    }

    func updateNSView(_ nsView: NSVisualEffectView, context: Context) {
        nsView.material = material
        nsView.blendingMode = blendingMode
    }
}

// MARK: - Editor Host

struct EditorHost: View {
    @Binding var code: String

    var body: some View {
        ZStack {
            Color.white
            ArdiumEditor(text: $code, fontSize: 14)
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity)
        .clipped()
    }
}

// MARK: - ContentView

struct ContentView: View {
    @StateObject private var ws = ArdiumWorkspace()

    @State private var showNewFile = false
    @State private var newFileName = ""
    @State private var showConsole = true

    var body: some View {
        NavigationSplitView {
            sidebar
        } detail: {
            main
        }
        .toolbar { toolbar }
        .sheet(isPresented: $showNewFile) {
            newFileSheet()
        }
    }

    private var sidebar: some View {
        ZStack {
            Glass(material: .sidebar)
            VStack(spacing: 0) {
                HStack {
                    Text("Documents/ArdiumProject")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                    Spacer()
                }
                .padding(.horizontal, 12)
                .padding(.top, 10)
                .padding(.bottom, 6)

                List(
                    selection: Binding(
                        get: { ws.currentFile },
                        set: { if let f = $0 { ws.selectFile(f) } }
                    )
                ) {
                    ForEach(ws.files, id: \.self) { f in
                        Label(f, systemImage: "doc.text")
                            .tag(Optional(f))
                            .contextMenu {
                                Button("Delete", role: .destructive) { ws.deleteFile(name: f) }
                            }
                    }
                }
                .listStyle(.sidebar)
            }
        }
        .navigationTitle("Ardium")
    }

    private var main: some View {
        VStack(spacing: 0) {
            HStack(spacing: 10) {
                Text(ws.currentFile ?? "No file selected")
                    .font(.headline)
                Spacer()
                Text(ws.statusText)
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }
            .padding(.horizontal, 12)
            .padding(.vertical, 10)
            .background(Glass(material: .headerView))

            Divider()

            Group {
                if ws.currentFile == nil {
                    emptyState
                } else {
                    EditorHost(code: $ws.code)
                }
            }
            .frame(maxWidth: .infinity, maxHeight: .infinity)

            if showConsole {
                Divider()
                ConsoleView(output: $ws.consoleOutput, isRunning: $ws.isRunning)
                    .frame(minHeight: 140)
            }
        }
    }

    private var emptyState: some View {
        VStack(spacing: 10) {
            Text("Select a file from the sidebar")
                .font(.title3)
                .foregroundStyle(.secondary)
            Button("Create main.ar") {
                ws.createFile(name: "main.ar")
                ws.selectFile("main.ar")
            }
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity)
        .background(Color.white)
    }

    @ToolbarContentBuilder
    private var toolbar: some ToolbarContent {
        ToolbarItemGroup(placement: .navigation) {
            Button {
                showNewFile = true
            } label: {
                Image(systemName: "plus")
            }
            .help("New file")

            Button {
                ws.saveCurrentFile()
            } label: {
                Image(systemName: "square.and.arrow.down")
            }
            .help("Save")
            .keyboardShortcut("s", modifiers: .command)
            .disabled(ws.currentFile == nil)
        }

        ToolbarItem(placement: .principal) {
            HStack(spacing: 10) {
                Button {
                    ws.run()
                } label: {
                    Image(systemName: "play.fill")
                        .foregroundStyle(.green)
                }
                .keyboardShortcut("r", modifiers: .command)
                .disabled(ws.currentFile == nil || ws.isRunning)
                .help("Run (Cmd+R)")

                Button {
                    ws.stop()
                } label: {
                    Image(systemName: "stop.fill")
                        .foregroundStyle(.red)
                }
                .disabled(!ws.isRunning)
                .help("Stop")
            }
            .padding(.horizontal, 10)
            .padding(.vertical, 6)
            .background(
                RoundedRectangle(cornerRadius: 10, style: .continuous)
                    .fill(Color(nsColor: .controlBackgroundColor).opacity(0.7))
            )
        }

        ToolbarItemGroup(placement: .primaryAction) {
            Button {
                showConsole.toggle()
            } label: {
                Image(systemName: showConsole ? "rectangle.bottomthird.inset.filled" : "rectangle")
            }
            .help("Toggle console")
        }
    }

    private func newFileSheet() -> some View {
        VStack(alignment: .leading, spacing: 12) {
            Text("New File")
                .font(.headline)

            TextField("Filename (e.g. test.ar)", text: $newFileName)
                .textFieldStyle(.roundedBorder)

            HStack {
                Spacer()
                Button("Cancel") {
                    newFileName = ""
                    showNewFile = false
                }
                Button("Create") {
                    ws.createFile(name: newFileName)
                    newFileName = ""
                    showNewFile = false
                }
                .keyboardShortcut(.defaultAction)
            }
        }
        .padding(16)
        .frame(width: 360)
    }
}
