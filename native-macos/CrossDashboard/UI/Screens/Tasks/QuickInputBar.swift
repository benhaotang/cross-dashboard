import SwiftUI
import CrossDashboardKit

/// Floating quick-input bar at the bottom of the tasks content column.
/// Shows a live parse preview (priority chip + due label) as the user types.
/// Mirrors QuickInputBar composable on Android.
struct QuickInputBar: View {

    @Binding var text: String
    var parsed: ParsedTask?
    var focusTrigger: Bool = false
    var isSubmitting: Bool = false
    var onSubmit: () -> Void

    @FocusState private var isFocused: Bool

    var body: some View {
        VStack(spacing: 0) {
            Divider()
            HStack(spacing: 10) {
                Image(systemName: "plus.circle.fill")
                    .foregroundStyle(.accent)
                    .font(.title3)

                TextField("Add a task… (!! priority, #tag, tomorrow)", text: $text)
                    .textFieldStyle(.plain)
                    .font(.body)
                    .focused($isFocused)
                    .onSubmit { onSubmit() }

                if isSubmitting {
                    ProgressView().controlSize(.small)
                } else if !text.isEmpty {
                    previewBadges
                    submitButton
                }
            }
            .padding(.horizontal, 16)
            .padding(.vertical, 10)
            .background(.regularMaterial)
        }
        .accessibilityElement(children: .contain)
        .onChange(of: focusTrigger) { _, shouldFocus in
            if shouldFocus {
                isFocused = true
            }
        }
    }

    // ─── Preview badges ───────────────────────────────────────────────────────

    @ViewBuilder
    private var previewBadges: some View {
        if let parsed {
            HStack(spacing: 6) {
                if parsed.priority > 0 {
                    PriorityChip(priority: parsed.priority)
                }
                if let due = parsed.due {
                    Text(due, style: .date)
                        .font(.caption)
                        .padding(.horizontal, 6)
                        .padding(.vertical, 3)
                        .background(.quaternary, in: Capsule())
                        .foregroundStyle(.secondary)
                }
                ForEach(parsed.categories, id: \.self) { tag in
                    TagChip(tag: tag)
                }
            }
        }
    }

    private var submitButton: some View {
        Button(action: onSubmit) {
            Image(systemName: "return")
                .font(.caption)
                .padding(6)
                .background(.accent.opacity(0.15), in: RoundedRectangle(cornerRadius: 6))
        }
        .buttonStyle(.plain)
        .accessibilityLabel("Submit task")
        .keyboardShortcut(.return, modifiers: [])
    }
}
