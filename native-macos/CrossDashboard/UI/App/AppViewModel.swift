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
    var selectedMemoID: String?

    func openTask(_ uid: String) {
        selectedTaskID = uid
        selectedScreen = .tasks
    }

    func openEvent(_ uid: String) {
        selectedEventID = uid
        selectedScreen = .events
    }

    func openIssue(_ id: Int64) {
        selectedIssueID = id
        selectedScreen = .issues
    }

    /// Returns visible screens in the user-defined order stored in preferences.
    var visibleScreens: [Screen] {
        preferences.visibleScreens.compactMap { Screen(rawValue: $0) }
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

    /// Set by `crossdashboard://capture?text=<encoded>` deep links.
    /// `MemosView` watches this and opens the compose sheet pre-filled.
    private(set) var captureInitialText: String? = nil

    func triggerCapture(text: String) {
        selectedScreen = .memos
        captureInitialText = text
    }

    func consumeCaptureTrigger() {
        captureInitialText = nil
    }

    /// Set by `crossdashboard://timer` when the user needs to choose a timer target.
    private(set) var isPomodoroPickerPresented = false
    private(set) var pomodoroPickerInitialName = ""

    func presentPomodoroPicker(initialName: String = "") {
        pomodoroPickerInitialName = initialName
        isPomodoroPickerPresented = true
    }

    func dismissPomodoroPicker() {
        isPomodoroPickerPresented = false
        pomodoroPickerInitialName = ""
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
    case memos     = "Capture"

    var id: String { rawValue }

    var systemImage: String {
        switch self {
        case .dashboard: return "square.grid.2x2"
        case .inbox:     return "tray"
        case .events:    return "calendar"
        case .tasks:     return "checklist"
        case .notes:     return "note.text"
        case .issues:    return "exclamationmark.bubble"
        case .views:     return "rectangle.3.group"
        case .memos:     return "camera.viewfinder"
        }
    }

    var label: String { rawValue }
}
