import SwiftUI
import Observation
import CrossDashboardKit

/// Central app-level state: theme, navigation selection, biometric lock, and cross-screen triggers.
/// Mirrors AppViewModel.kt on Android.
@Observable
@MainActor
final class AppViewModel {

    // ─── Dependencies ─────────────────────────────────────────────────────────
    private let preferences: AppPreferences = .shared

    // ─── Theme ────────────────────────────────────────────────────────────────

    var theme: ThemePreference {
        get { preferences.theme }
        set { preferences.theme = newValue }
    }

    var colorScheme: ColorScheme? {
        switch preferences.theme {
        case .light:  return .light
        case .dark:   return .dark
        case .system: return nil
        }
    }

    // ─── Navigation ───────────────────────────────────────────────────────────

    var selectedScreen: Screen? = .dashboard
    var selectedTaskID: String?
    var selectedEventID: String?
    var selectedNoteID: String?
    var selectedIssueID: Int64?

    var visibleScreens: [Screen] {
        Screen.allCases.filter { preferences.visibleScreens.contains($0.rawValue) }
    }

    // ─── Biometric lock ───────────────────────────────────────────────────────

    var isLocked: Bool = false

    func lock() { isLocked = true }
    func unlock() { isLocked = false }

    // ─── Cross-screen triggers ────────────────────────────────────────────────

    private(set) var newTaskRequested: Bool = false

    func triggerNewTask() {
        selectedScreen = .tasks
        newTaskRequested = true
    }

    func consumeNewTaskTrigger() {
        newTaskRequested = false
    }
}

// ─── Screen enum ─────────────────────────────────────────────────────────────

enum Screen: String, CaseIterable, Identifiable, Hashable {
    case dashboard = "Dashboard"
    case inbox     = "Inbox"
    case events    = "Events"
    case tasks     = "Tasks"
    case notes     = "Notes"
    case issues    = "Issues"
    case views     = "Views"

    var id: String { rawValue }

    var systemImage: String {
        switch self {
        case .dashboard: return "square.grid.2x2"
        case .inbox:     return "tray"
        case .events:    return "calendar"
        case .tasks:     return "checklist"
        case .notes:     return "note.text"
        case .issues:    return "exclamationmark.bubble"
        case .views:     return "kanban"
        }
    }

    var label: String { rawValue }
}
