import AppKit
import SwiftUI

struct ArcEditor: NSViewRepresentable {
    @Binding var text: String
    var fileExtension: String = "ar"
    var fontSize: CGFloat = 13.5

    func makeNSView(context: Context) -> NSScrollView {
        let scrollView = NSTextView.scrollableTextView()

        scrollView.drawsBackground = true
        scrollView.backgroundColor = NSColor.textBackgroundColor
        scrollView.borderType = .noBorder
        scrollView.hasVerticalScroller = true
        scrollView.hasHorizontalRuler = false
        scrollView.autohidesScrollers = true

        guard let textView = scrollView.documentView as? NSTextView else {
            return scrollView
        }

        textView.font = NSFont.monospacedSystemFont(ofSize: fontSize, weight: .regular)
        textView.backgroundColor = NSColor.textBackgroundColor
        textView.textColor = NSColor.textColor
        textView.insertionPointColor = NSColor.textColor

        textView.isEditable = true
        textView.isSelectable = true
        textView.isRichText = false
        textView.importsGraphics = false
        textView.isAutomaticQuoteSubstitutionEnabled = false
        textView.isAutomaticDashSubstitutionEnabled = false
        textView.isAutomaticSpellingCorrectionEnabled = false

        textView.isVerticallyResizable = true
        textView.isHorizontallyResizable = false
        textView.autoresizingMask = [.width]
        textView.textContainer?.widthTracksTextView = true
        textView.textContainer?.containerSize = NSSize(
            width: CGFloat.greatestFiniteMagnitude, height: CGFloat.greatestFiniteMagnitude)

        textView.delegate = context.coordinator
        context.coordinator.textView = textView

        let ruler = LineNumberRulerView(textView: textView)
        scrollView.verticalRulerView = ruler
        scrollView.rulersVisible = true

        textView.string = text
        context.coordinator.applyHighlights()

        return scrollView
    }

    func updateNSView(_ nsView: NSScrollView, context: Context) {
        guard let textView = nsView.documentView as? NSTextView else { return }

        if textView.string != text {
            let selectedRanges = textView.selectedRanges
            textView.string = text
            if let firstRange = selectedRanges.first as? NSRange,
                firstRange.location <= text.count
            {
                textView.selectedRanges = selectedRanges
            }
            context.coordinator.applyHighlights()
        }
    }

    func makeCoordinator() -> Coordinator {
        Coordinator(self)
    }

    class Coordinator: NSObject, NSTextViewDelegate {
        var parent: ArcEditor
        weak var textView: NSTextView?

        init(_ parent: ArcEditor) {
            self.parent = parent
        }

        func textDidChange(_ notification: Notification) {
            guard let textView = notification.object as? NSTextView else { return }
            self.parent.text = textView.string
            applyHighlights()
            textView.enclosingScrollView?.verticalRulerView?.needsDisplay = true
        }

        func applyHighlights() {
            guard let textView = textView else { return }
            let text = textView.string
            let nsString = text as NSString
            let range = NSRange(location: 0, length: nsString.length)
            let storage = textView.textStorage

            storage?.beginEditing()

            // 1. Reset
            storage?.removeAttribute(.foregroundColor, range: range)
            storage?.addAttribute(.foregroundColor, value: NSColor.labelColor, range: range)
            storage?.addAttribute(
                .font,
                value: NSFont.monospacedSystemFont(ofSize: parent.fontSize, weight: .regular),
                range: range)

            // 2. Syntax Highlighting (Strings, Keywords, Numbers, Types, Comments)
            let isDark = NSApp.effectiveAppearance.name == .darkAqua || NSApp.effectiveAppearance.name == .vibrantDark

            let stringColor = isDark ? NSColor(red: 0.98, green: 0.55, blue: 0.40, alpha: 1.0) : NSColor(red: 0.85, green: 0.20, blue: 0.15, alpha: 1.0)
            let keywordColor = isDark ? NSColor(red: 0.92, green: 0.40, blue: 0.85, alpha: 1.0) : NSColor(red: 0.65, green: 0.10, blue: 0.60, alpha: 1.0)
            let typeColor = isDark ? NSColor(red: 0.40, green: 0.75, blue: 0.98, alpha: 1.0) : NSColor(red: 0.05, green: 0.40, blue: 0.80, alpha: 1.0)
            let numberColor = isDark ? NSColor(red: 0.85, green: 0.75, blue: 0.40, alpha: 1.0) : NSColor(red: 0.70, green: 0.50, blue: 0.05, alpha: 1.0)
            let commentColor = isDark ? NSColor(red: 0.50, green: 0.55, blue: 0.60, alpha: 1.0) : NSColor(red: 0.45, green: 0.50, blue: 0.55, alpha: 1.0)

            // Strings
            highlight(pattern: "\"[^\"]*\"", color: stringColor)
            highlight(pattern: "'[^']*'", color: stringColor)

            // Keywords (Multi-language: Arc, C/C++, Swift, Python, Rust, JS)
            highlight(
                pattern: "\\b(fn|let|mut|struct|import|from|def|class|interface|if|else|elif|return|var|const|while|for|in|loop|match|case|pub|extern|void|async|await|try|catch|throw)\\b",
                color: keywordColor)

            // Types & Built-ins
            highlight(
                pattern: "\\b(VStack|HStack|ZStack|Button|Title|Subtitle|Spacer|App|println|print|String|Int|Float|Bool|Void|i64|i32|u64|u32|f64|f32|self|Self|true|false|nil|null|None)\\b",
                color: typeColor)

            // Numbers
            highlight(pattern: "\\b\\d+(\\.\\d+)?\\b", color: numberColor)

            // Comments
            highlight(pattern: "//.*", color: commentColor)
            highlight(pattern: "#.*", color: commentColor)

            storage?.endEditing()
        }

        private func highlight(pattern: String, color: NSColor) {
            guard let storage = textView?.textStorage else { return }
            let string = storage.string
            let range = NSRange(location: 0, length: string.utf16.count)

            guard let regex = try? NSRegularExpression(pattern: pattern, options: []) else {
                return
            }

            regex.enumerateMatches(in: string, options: [], range: range) { match, _, _ in
                if let matchRange = match?.range {
                    storage.addAttribute(.foregroundColor, value: color, range: matchRange)
                }
            }
        }
    }
}


// --- Custom Ruler View ---
class LineNumberRulerView: NSRulerView {
    init(textView: NSTextView) {
        super.init(scrollView: textView.enclosingScrollView!, orientation: .verticalRuler)
        self.clientView = textView
        self.ruleThickness = 35
    }

    required init(coder: NSCoder) { fatalError() }

    override func drawHashMarksAndLabels(in rect: NSRect) {
        // Background
        NSColor(white: 0.95, alpha: 1.0).setFill()
        rect.fill()

        // Divider Line
        let path = NSBezierPath()
        path.move(to: NSPoint(x: ruleThickness, y: rect.minY))
        path.line(to: NSPoint(x: ruleThickness, y: rect.maxY))
        NSColor.lightGray.setStroke()
        path.stroke()

        guard let textView = self.clientView as? NSTextView,
            let layoutManager = textView.layoutManager,
            let textContainer = textView.textContainer
        else { return }

        let visibleRect = self.scrollView?.documentVisibleRect ?? .zero
        let range = layoutManager.glyphRange(forBoundingRect: visibleRect, in: textContainer)
        let textString = textView.string as NSString
        if textString.length == 0 { return }

        // Find start line number
        let startChar = layoutManager.characterIndexForGlyph(at: range.location)
        let safeIndex = min(startChar, textString.length)
        let prefixString = textString.substring(to: safeIndex)
        var lineNumber = prefixString.filter({ $0 == "\n" }).count + 1

        var glyphIndex = range.location
        while glyphIndex < NSMaxRange(range) {
            let charIndex = layoutManager.characterIndexForGlyph(at: glyphIndex)
            let lineRange = textString.lineRange(for: NSRange(location: charIndex, length: 0))

            let lineRect = layoutManager.lineFragmentRect(
                forGlyphAt: glyphIndex, effectiveRange: nil)

            // Vertical Adjustment
            var yPos = self.convert(lineRect.origin, from: textView).y
            yPos += textView.textContainerInset.height

            // Draw Number
            let numStr = "\(lineNumber)" as NSString
            let attrs: [NSAttributedString.Key: Any] = [
                .font: NSFont.monospacedSystemFont(ofSize: 10, weight: .regular),
                .foregroundColor: NSColor.gray,
            ]
            let size = numStr.size(withAttributes: attrs)
            numStr.draw(
                at: NSPoint(x: ruleThickness - size.width - 5, y: yPos + 2), withAttributes: attrs)

            lineNumber += 1
            glyphIndex = NSMaxRange(
                layoutManager.glyphRange(forCharacterRange: lineRange, actualCharacterRange: nil))
        }
    }
}
