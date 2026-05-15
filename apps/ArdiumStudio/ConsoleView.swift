import SwiftUI

struct ConsoleView: View {
    @Binding var output: String
    @Binding var isRunning: Bool

    var body: some View {
        VStack(spacing: 0) {
            // Minimal Shell Header
            HStack {
                Text("Shell")
                    .font(.system(size: 11, weight: .medium))
                    .foregroundColor(.secondary)

                Spacer()

                if isRunning {
                    ProgressView()
                        .controlSize(.mini)
                        .padding(.trailing, 4)
                    Text("Busy")
                        .font(.system(size: 10))
                        .foregroundColor(.secondary)
                }

                Button(action: { output = "" }) {
                    Image(systemName: "trash")
                        .foregroundColor(.secondary)
                }
                .buttonStyle(.plain)
                .help("Clear text")
                .padding(.leading, 8)
            }
            .padding(.horizontal, 10)
            .frame(height: 24)
            .background(Color(nsColor: .controlBackgroundColor))
            .overlay(Divider(), alignment: .bottom)

            // Shell Output Area
            ScrollViewReader { proxy in
                ScrollView {
                    Text(output.isEmpty ? "Ardium 2.5.5\n>>> " : output)
                        .font(.system(.body, design: .monospaced))
                        .foregroundColor(Color(nsColor: .labelColor))
                        .frame(maxWidth: .infinity, alignment: .leading)
                        .padding(6)
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
