import SwiftUI
import CrossDashboardKit

/// Pill badge showing a CalDavTask status (NEEDS-ACTION, IN-PROCESS, COMPLETED, CANCELLED).
/// Mirrors StatusBadge composable on Android.
struct StatusBadge: View {

    let status: TaskStatus

    var body: some View {
        Text(status.label)
            .font(.caption)
            .fontWeight(.medium)
            .padding(.horizontal, 8)
            .padding(.vertical, 3)
            .background(backgroundColor.opacity(0.15), in: Capsule())
            .foregroundStyle(backgroundColor)
            .accessibilityLabel("Status: \(status.label)")
    }

    private var backgroundColor: Color {
        switch status {
        case .needsAction: return .blue
        case .inProcess:   return .orange
        case .completed:   return .green
        case .cancelled:   return .secondary
        }
    }
}

private extension TaskStatus {
    var label: String {
        switch self {
        case .needsAction: return "Needs Action"
        case .inProcess:   return "In Progress"
        case .completed:   return "Completed"
        case .cancelled:   return "Cancelled"
        }
    }
}
