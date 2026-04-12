import SwiftUI
import CrossDashboardKit

/// Root three-column NavigationSplitView.
/// Sidebar → screen list, Content → list/collection for selected screen, Detail → item detail.
/// Mirrors the NavigationSuiteScaffold + NavigableListDetailPaneScaffold pattern on Android.
struct ContentView: View {

    @Environment(AppViewModel.self) private var appViewModel
    @Environment(PomodoroViewModel.self) private var pomodoroVM
    @Environment(\.appContainer) private var container

    @State private var columnVisibility: NavigationSplitViewVisibility = .all
    @State private var selectedScreen: Screen? = .dashboard

    var body: some View {
        Group {
            if appViewModel.isLocked {
                BiometricLockView()
            } else {
                splitView
                    .overlay(alignment: .bottomTrailing) {
                        PomodoroFloatingBar()
                    }
                    .onChange(of: selectedScreen) { _, newScreen in
                        appViewModel.selectedScreen = newScreen
                    }
                    .onChange(of: appViewModel.selectedScreen) { _, newScreen in
                        selectedScreen = newScreen
                    }
                    .onReceive(NotificationCenter.default.publisher(for: .crossDashboardOpenEvent)) { note in
                        if let uid = note.userInfo?["eventUID"] as? String {
                            selectedScreen = .events
                            appViewModel.selectedEventID = uid
                        }
                    }
            }
        }
    }

    /// Views doesn't use a detail pane, so we switch to a two-column split to
    /// avoid the empty "Select an item" placeholder eating half the screen.
    @ViewBuilder
    private var splitView: some View {
        if selectedScreen == .views {
            NavigationSplitView(columnVisibility: $columnVisibility) {
                sidebar
            } detail: {
                ViewsView()
            }
            .navigationSplitViewStyle(.balanced)
        } else {
            NavigationSplitView(columnVisibility: $columnVisibility) {
                sidebar
            } content: {
                contentColumn
            } detail: {
                detailColumn
            }
            .navigationSplitViewStyle(.balanced)
        }
    }

    // ─── Sidebar ──────────────────────────────────────────────────────────────

    private var sidebar: some View {
        List {
            ForEach(appViewModel.visibleScreens) { screen in
                Label(screen.label, systemImage: screen.systemImage)
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .contentShape(Rectangle())
                    .listRowBackground(
                        selectedScreen == screen
                            ? Color.accentColor.opacity(0.15)
                            : Color.clear
                    )
                    .foregroundStyle(selectedScreen == screen ? Color.accentColor : Color.primary)
                    .onTapGesture { selectedScreen = screen }
                    .accessibilityLabel(screen.label)
                    .accessibilityAddTraits(selectedScreen == screen ? .isSelected : [])
            }
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
        switch selectedScreen {
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
            EmptyView()
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
        switch selectedScreen {
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
