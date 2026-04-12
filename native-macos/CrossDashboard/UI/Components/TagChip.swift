import SwiftUI
import CrossDashboardKit

/// Small rounded pill showing a single tag/category string.
/// Mirrors TagChip / KanbanChip composable on Android.
struct TagChip: View {

    let tag: String
    var color: Color = .accentColor

    var body: some View {
        Text("#\(tag)")
            .font(.caption2)
            .fontWeight(.medium)
            .foregroundStyle(color)
            .padding(.horizontal, 6)
            .padding(.vertical, 3)
            .background(color.opacity(0.12), in: Capsule())
            .accessibilityLabel("Tag: \(tag)")
    }
}
