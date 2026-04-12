import SwiftUI
import MarkdownUI
import CrossDashboardKit

// ─── ReadMarkdownView ─────────────────────────────────────────────────────────
// Unlabeled markdown renderer — drop-in for plain Text() in read-only bodies.
// Mirrors MarkdownText composable on Android.

struct ReadMarkdownView: View {
    let content: String

    var body: some View {
        Markdown(content)
            .markdownTheme(.gitHub)
            .markdownTextStyle {
                ForegroundColor(.primary)
            }
            .frame(maxWidth: .infinity, alignment: .leading)
    }
}

// ─── ReadMarkdownField ────────────────────────────────────────────────────────
// Labeled field that renders its value as GFM markdown.
// Drop-in replacement for ReadField when the value may contain markdown.
// Mirrors ReadMarkdownField composable on Android.

struct ReadMarkdownField: View {
    let label: String
    let content: String

    var body: some View {
        VStack(alignment: .leading, spacing: 4) {
            Text(label)
                .font(.caption)
                .fontDesign(.monospaced)
                .textCase(.uppercase)
                .foregroundStyle(.secondary)

            ReadMarkdownView(content: content)
        }
    }
}
