import SwiftUI

struct ConsoleView: View {
    @Binding var output: String
    @Binding var isRunning: Bool

    var body: some View {
        VStack(spacing: 0) {
            // Minimal Shell Header
            HStack {
                Image(systemName: "terminal.fill")
                    .font(.system(size: 10))
                    .foregroundColor(.secondary)
                Text("Terminal Output")
                    .font(.system(size: 11, weight: .medium))
                    .foregroundColor(.secondary)

                Spacer()

                if isRunning {
                    ProgressView()
                        .controlSize(.mini)
                        .padding(.trailing, 4)
                    Text("Running...")
                        .font(.system(size: 10))
                        .foregroundColor(.secondary)
                }

                Button(action: { output = "" }) {
                    Image(systemName: "trash")
                        .foregroundColor(.secondary)
                }
                .buttonStyle(.plain)
                .help("Clear Terminal")
                .padding(.leading, 8)
            }
            .padding(.horizontal, 12)
            .frame(height: 26)
            .background(Color(nsColor: .controlBackgroundColor))
            .overlay(Divider(), alignment: .bottom)

            // Shell Output Area
            ScrollViewReader { proxy in
                ScrollView {
                    Text(output.isEmpty ? "Arc Studio IDE v3.0.0\n>>> Ready.\n" : output)
                        .font(.system(size: 12, weight: .regular, design: .monospaced))
                        .foregroundColor(Color(nsColor: .labelColor))
                        .frame(maxWidth: .infinity, alignment: .leading)
                        .padding(8)
                        .textSelection(.enabled)
                        .id("bottom")
                }
                .background(Color(nsColor: .textBackgroundColor))
                .onChange(of: output) { _ in
                    proxy.scrollTo("bottom", anchor: .bottom)
                }
            }
        }
    }
}

