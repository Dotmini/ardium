import AppKit
import SwiftUI

// MARK: - Workspace

@MainActor
final class ArcWorkspace: ObservableObject {
    @Published var code: String = ""
    @Published var consoleOutput: String = "Arc Studio IDE\n>>> Ready.\n"
    @Published var isRunning: Bool = false
    @Published var statusText: String = "Ready"

    @Published var files: [String] = []
    @Published var currentFile: String? = nil

    let projectURL: URL

    init() {
        let docs = FileManager.default.urls(for: .documentDirectory, in: .userDomainMask).first!
        self.projectURL = docs.appendingPathComponent("ArcProject", isDirectory: true)

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
                // Arc Language - High Performance Native Program
                fn main() {
                    println("Hello from Arc Studio IDE! 🚀");
                    return 0;
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
            .filter { ["ar", "swift", "py", "c", "cpp", "rs", "js", "ts", "toml", "json", "sh"].contains($0.pathExtension.lowercased()) }
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
            var starter = ""
            let ext = (n as NSString).pathExtension.lowercased()
            if ext == "ar" {
                starter = "fn main() {\n    println(\"Hello, Arc!\");\n    return 0;\n}\n"
            } else if ext == "py" {
                starter = "print('Hello from Python!')\n"
            } else if ext == "swift" {
                starter = "import Foundation\nprint(\"Hello from Swift!\")\n"
            } else if ext == "c" || ext == "cpp" {
                starter = "#include <stdio.h>\n\nint main() {\n    printf(\"Hello from C!\\n\");\n    return 0;\n}\n"
            }

            try starter.write(to: url, atomically: true, encoding: .utf8)
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

    // MARK: Run Multi-Language Execution
    func run() {
        guard !isRunning else { return }
        guard let f = currentFile else { return }

        saveCurrentFile()

        isRunning = true
        statusText = "Running…"
        consoleOutput = ">>> Executing \(f)...\n"

        let fileURL = projectURL.appendingPathComponent(f)
        let ext = fileURL.pathExtension.lowercased()

        Task.detached { [weak self] in
            guard let self else { return }

            let task = Process()
            let pipe = Pipe()
            task.standardOutput = pipe
            task.standardError = pipe

            if ext == "ar" {
                let home = FileManager.default.homeDirectoryForCurrentUser.path
                let arcCandidates = [
                    "\(home)/.cargo/bin/arc",
                    "/usr/local/bin/arc",
                    "/opt/homebrew/bin/arc",
                    "/Users/dotmini/Documents/ardium/arc"
                ]
                let arcPath = arcCandidates.first(where: { FileManager.default.fileExists(atPath: $0) }) ?? "/usr/local/bin/arc"
                task.executableURL = URL(fileURLWithPath: arcPath)
                task.arguments = ["run", fileURL.path]
            } else if ext == "py" {
                task.executableURL = URL(fileURLWithPath: "/usr/bin/python3")
                task.arguments = [fileURL.path]
            } else if ext == "swift" {
                task.executableURL = URL(fileURLWithPath: "/usr/bin/swift")
                task.arguments = [fileURL.path]
            } else if ext == "c" || ext == "cpp" {
                let binOut = "/tmp/arc_studio_c_out"
                let compileCmd = "\(ext == "c" ? "clang" : "clang++") \(fileURL.path) -o \(binOut) && \(binOut)"
                task.executableURL = URL(fileURLWithPath: "/bin/zsh")
                task.arguments = ["-c", compileCmd]
            } else if ext == "sh" {
                task.executableURL = URL(fileURLWithPath: "/bin/bash")
                task.arguments = [fileURL.path]
            } else {
                task.executableURL = URL(fileURLWithPath: "/bin/zsh")
                task.arguments = ["-c", "cat \(fileURL.path)"]
            }

            do {
                try task.run()
                task.waitUntilExit()

                let data = pipe.fileHandleForReading.readDataToEndOfFile()
                let out = String(data: data, encoding: .utf8) ?? ""

                await MainActor.run {
                    self.consoleOutput += out
                    self.consoleOutput += "\n>>> Process finished with exit code \(task.terminationStatus)\n"
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
        isRunning = false
        statusText = "Stopped"
        consoleOutput += ">>> Process stopped by user\n"
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
    var currentFile: String?

    var body: some View {
        let ext = (currentFile as NSString?)?.pathExtension.lowercased() ?? "ar"
        ArcEditor(text: $code, fileExtension: ext, fontSize: 13.5)
            .frame(maxWidth: .infinity, maxHeight: .infinity)
            .clipped()
    }
}

// MARK: - ContentView

struct ContentView: View {
    @StateObject private var ws = ArcWorkspace()

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
                    Image(systemName: "folder.fill")
                        .foregroundStyle(.blue)
                    Text("Arc Workspace")
                        .font(.system(size: 11, weight: .semibold))
                        .foregroundStyle(.secondary)
                    Spacer()
                }
                .padding(.horizontal, 12)
                .padding(.top, 12)
                .padding(.bottom, 6)

                List(
                    selection: Binding(
                        get: { ws.currentFile },
                        set: { if let f = $0 { ws.selectFile(f) } }
                    )
                ) {
                    ForEach(ws.files, id: \.self) { f in
                        HStack(spacing: 6) {
                            fileIcon(for: f)
                            Text(f)
                                .font(.system(size: 12))
                        }
                        .tag(Optional(f))
                        .contextMenu {
                            Button("Delete", role: .destructive) { ws.deleteFile(name: f) }
                        }
                    }
                }
                .listStyle(.sidebar)
            }
        }
        .navigationTitle("Arc Studio")
    }

    @ViewBuilder
    private func fileIcon(for filename: String) -> some View {
        let ext = (filename as NSString).pathExtension.lowercased()
        switch ext {
        case "ar":
            Image(systemName: "bolt.fill").foregroundStyle(.cyan)
        case "swift":
            Image(systemName: "swift").foregroundStyle(.orange)
        case "py":
            Image(systemName: "chevron.left.forwardslash.chevron.right").foregroundStyle(.yellow)
        case "c", "cpp":
            Image(systemName: "c.circle.fill").foregroundStyle(.blue)
        case "rs":
            Image(systemName: "gearshape.fill").foregroundStyle(.brown)
        default:
            Image(systemName: "doc.text.fill").foregroundStyle(.secondary)
        }
    }


    private var main: some View {
        VStack(spacing: 0) {
            HStack(spacing: 10) {
                if let file = ws.currentFile {
                    fileIcon(for: file)
                    Text(file)
                        .font(.system(size: 13, weight: .medium))
                } else {
                    Text("No file selected")
                        .font(.system(size: 13, weight: .medium))
                        .foregroundStyle(.secondary)
                }
                Spacer()
                Text(ws.statusText)
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }
            .padding(.horizontal, 14)
            .padding(.vertical, 8)
            .background(Glass(material: .headerView))

            Divider()

            Group {
                if ws.currentFile == nil {
                    emptyState
                } else {
                    EditorHost(code: $ws.code, currentFile: ws.currentFile)
                }
            }
            .frame(maxWidth: .infinity, maxHeight: .infinity)

            if showConsole {
                Divider()
                ConsoleView(output: $ws.consoleOutput, isRunning: $ws.isRunning)
                    .frame(minHeight: 160)
            }
        }
    }

    private var emptyState: some View {
        VStack(spacing: 12) {
            Image(systemName: "chevron.left.forwardslash.chevron.right")
                .font(.system(size: 40))
                .foregroundStyle(.secondary.opacity(0.6))
            Text("Select a file or create a new one to begin")
                .font(.title3)
                .foregroundStyle(.secondary)
            Button("Create main.ar") {
                ws.createFile(name: "main.ar")
                ws.selectFile("main.ar")
            }
            .buttonStyle(.borderedProminent)
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity)
        .background(Color(nsColor: .textBackgroundColor))
    }

    @ToolbarContentBuilder
    private var toolbar: some ToolbarContent {
        ToolbarItemGroup(placement: .navigation) {
            Button {
                showNewFile = true
            } label: {
                Image(systemName: "plus")
            }
            .help("New File (Cmd+N)")
            .keyboardShortcut("n", modifiers: .command)

            Button {
                ws.saveCurrentFile()
            } label: {
                Image(systemName: "square.and.arrow.down")
            }
            .help("Save (Cmd+S)")
            .keyboardShortcut("s", modifiers: .command)
            .disabled(ws.currentFile == nil)
        }

        ToolbarItem(placement: .principal) {
            HStack(spacing: 12) {
                Button {
                    ws.run()
                } label: {
                    HStack(spacing: 4) {
                        Image(systemName: "play.fill")
                            .font(.system(size: 11))
                        Text("Run")
                            .font(.system(size: 12, weight: .medium))
                    }
                    .foregroundStyle(.green)
                }
                .keyboardShortcut("r", modifiers: .command)
                .disabled(ws.currentFile == nil || ws.isRunning)
                .help("Build & Run (Cmd+R)")

                if ws.isRunning {
                    Button {
                        ws.stop()
                    } label: {
                        Image(systemName: "stop.fill")
                            .foregroundStyle(.red)
                    }
                    .help("Stop Execution")
                }
            }
            .padding(.horizontal, 10)
            .padding(.vertical, 4)
            .background(
                RoundedRectangle(cornerRadius: 8, style: .continuous)
                    .fill(Color(nsColor: .controlBackgroundColor).opacity(0.8))
            )
        }

        ToolbarItemGroup(placement: .primaryAction) {
            Button {
                showConsole.toggle()
            } label: {
                Image(systemName: showConsole ? "rectangle.bottomthird.inset.filled" : "rectangle")
            }
            .help("Toggle Output Terminal")
        }
    }

    private func newFileSheet() -> some View {
        VStack(alignment: .leading, spacing: 14) {
            Text("Create New File")
                .font(.headline)

            TextField("Filename (e.g. main.ar, script.py, demo.swift)", text: $newFileName)
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
        .padding(18)
        .frame(width: 380)
    }
}

