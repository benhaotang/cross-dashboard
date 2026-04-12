import SwiftUI
import CrossDashboardKit

/// Small chip showing task priority (high / medium / low).
/// Priority 0 renders nothing. Mirrors PriorityChip composable on Android.
struct PriorityChip: View {

    /// RFC 5545 priority: 0 = none, 1–4 = high, 5 = medium, 6–9 = low
    let priority: Int

    var body: some View {
        if let info = priorityInfo {
            Label(info.label, systemImage: info.icon)
                .font(.caption2)
                .fontWeight(.semibold)
                .foregroundStyle(info.color)
                .padding(.horizontal, 6)
                .padding(.vertical, 3)
                .background(info.color.opacity(0.12), in: Capsule())
                .accessibilityLabel("Priority: \(info.label)")
        }
    }

    private var priorityInfo: (label: String, icon: String, color: Color)? {
        switch priority {
        case 1...4: return ("High",   "exclamationmark.2",  .red)
        case 5:     return ("Medium", "exclamationmark",    .orange)
        case 6...9: return ("Low",    "arrow.down",         .blue)
        default:    return nil
        }
    }
}
