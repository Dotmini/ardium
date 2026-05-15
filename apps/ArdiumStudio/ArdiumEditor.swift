import AppKit
import SwiftUI

struct ArdiumEditor: NSViewRepresentable {
    @Binding var text: String
    var fontSize: CGFloat = 14

    func makeNSView(context: Context) -> NSScrollView {
        // 1. Use the standard factory method to ensure the Text System stack is wired correctly.
        let scrollView = NSTextView.scrollableTextView()

        // UI Tweaks for the ScrollView
        scrollView.drawsBackground = true
        scrollView.backgroundColor = .white
        scrollView.borderType = .noBorder
        scrollView.hasVerticalScroller = true
        scrollView.hasHorizontalRuler = false
        scrollView.autohidesScrollers = true

        guard let textView = scrollView.documentView as? NSTextView else {
            return scrollView
        }

        // 2. Configure TextView for Code Editing
        textView.font = NSFont.monospacedSystemFont(ofSize: fontSize, weight: .regular)
        textView.backgroundColor = .white
        textView.textColor = .black
        textView.insertionPointColor = .black

        // Editing Properties
        textView.isEditable = true
        textView.isSelectable = true
        textView.isRichText = false
        textView.importsGraphics = false
        textView.isAutomaticQuoteSubstitutionEnabled = false
        textView.isAutomaticDashSubstitutionEnabled = false
        textView.isAutomaticSpellingCorrectionEnabled = false

        // Layout Constraints
        textView.isVerticallyResizable = true
        textView.isHorizontallyResizable = false
        textView.autoresizingMask = [.width]
        textView.textContainer?.widthTracksTextView = true
        textView.textContainer?.containerSize = NSSize(
            width: CGFloat.greatestFiniteMagnitude, height: CGFloat.greatestFiniteMagnitude)

        // 3. Delegate & Context
        textView.delegate = context.coordinator
        context.coordinator.textView = textView

        // 4. Line Number Ruler
        let ruler = LineNumberRulerView(textView: textView)
        scrollView.verticalRulerView = ruler
        scrollView.rulersVisible = true

        // 5. Initial State
        textView.string = text
        context.coordinator.applyHighlights()

        return scrollView
    }

    func updateNSView(_ nsView: NSScrollView, context: Context) {
        guard let textView = nsView.documentView as? NSTextView else { return }

        if textView.string != text {
            // Save cursor
            let selectedRanges = textView.selectedRanges

            // Update text
            textView.string = text

            // Restore cursor (if within bounds)
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
        var parent: ArdiumEditor
        weak var textView: NSTextView?

        init(_ parent: ArdiumEditor) {
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

            // 1. Reset to Base (Black)
            storage?.removeAttribute(.foregroundColor, range: range)
            storage?.addAttribute(.foregroundColor, value: NSColor.black, range: range)
            storage?.addAttribute(
                .font,
                value: NSFont.monospacedSystemFont(ofSize: parent.fontSize, weight: .regular),
                range: range)

            // 2. Syntax Highlighting

            // Strings (Red)
            highlight(
                pattern: "\"[^\"]*\"", color: NSColor(red: 0.8, green: 0.0, blue: 0.0, alpha: 1.0))

            // Keywords (Purple)
            highlight(
                pattern:
                    "\\b(fn|let|mut|struct|import|VClass|HClass|ZClass|if|else|return|var|init)\\b",
                color: NSColor(red: 0.5, green: 0.0, blue: 0.5, alpha: 1.0))

            // Types (Dark Blue)
            highlight(
                pattern: "\\b(Button|String|Int|Bool|Void)\\b",
                color: NSColor(red: 0.0, green: 0.0, blue: 0.6, alpha: 1.0))

            // Comments (Gray)
            highlight(pattern: "//.*", color: NSColor.gray)

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
