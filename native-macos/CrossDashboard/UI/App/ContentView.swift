import SwiftUI
import CrossDashboardKit

/// Root three-column NavigationSplitView.
/// Sidebar → screen list, Content → list/collection for selected screen, Detail → item detail.
/// Mirrors the NavigationSuiteScaffold + NavigableListDetailPaneScaffold pattern on Android.
struct ContentView: View {

    @Environment(AppViewModel.self) private var appViewModel
    @Environment(PomodoroViewModel.self) private var pomodoroVM
    @Environment(\.appContainer) private var container

    var body: some View {
        @Bindable var vm = appViewModel

        Group {
            if appViewModel.isLocked {
                BiometricLockView()
            } else {
                NavigationSplitView(columnVisibility: .constant(.all)) {
                    sidebar
                } content: {
                    contentColumn
                } detail: {
                    detailColumn
                }
                .navigationSplitViewStyle(.balanced)
                .overlay(alignment: .bottomTrailing) {
                    PomodoroFloatingBar()
                }
                .onReceive(NotificationCenter.default.publisher(for: .crossDashboardOpenEvent)) { note in
                    if let uid = note.userInfo?["eventUID"] as? String {
                        appViewModel.selectedScreen = .events
                        appViewModel.selectedEventID = uid
                    }
                }
            }
        }
    }

    // ─── Sidebar ──────────────────────────────────────────────────────────────

    private var sidebar: some View {
        @Bindable var vm = appViewModel
        return List(appViewModel.visibleScreens, selection: $vm.selectedScreen) { screen in
            Label(screen.label, systemImage: screen.systemImage)
                .accessibilityLabel(screen.label)
        }
        .listStyle(.sidebar)
        .navigationTitle("CrossDashboard")
        .toolbar {
            ToolbarItem(placement: .primaryAction) {
                Button {
                    Task { await container.syncAll() }
                } label: {
                    Label("Sync", systemImage: "arrow.clockwise")
                }
                .accessibilityLabel("Sync now")
                .keyboardShortcut("r", modifiers: .command)
            }
        }
    }

    // ─── Content column ───────────────────────────────────────────────────────

    @ViewBuilder
    private var contentColumn: some View {
        switch appViewModel.selectedScreen {
        case .dashboard:
            DashboardView()
        case .inbox:
            InboxView()
        case .events:
            EventsView()
        case .tasks:
            TasksView()
        case .notes:
            NotesView()
        case .issues:
            IssuesView()
        case .views:
            ViewsView()
        case nil:
            ContentUnavailableView(
                "CrossDashboard",
                systemImage: "square.grid.2x2",
                description: Text("Select a screen from the sidebar.")
            )
        }
    }

    // ─── Detail column ────────────────────────────────────────────────────────

    @ViewBuilder
    private var detailColumn: some View {
        switch appViewModel.selectedScreen {
        case .tasks:
            TaskDetailView(taskID: appViewModel.selectedTaskID)
        case .events:
            EventDetailView(eventID: appViewModel.selectedEventID)
        case .notes:
            NoteDetailView(noteID: appViewModel.selectedNoteID)
        case .issues:
            IssueDetailView(issueID: appViewModel.selectedIssueID)
        default:
            ContentUnavailableView(
                "Select an item",
                systemImage: "sidebar.right",
                description: Text("Choose an item from the list to see its details.")
            )
        }
    }

}
