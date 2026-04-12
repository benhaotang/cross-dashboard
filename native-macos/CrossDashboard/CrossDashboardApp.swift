import SwiftUI
import CrossDashboardKit

@main
struct CrossDashboardApp: App {

    @State private var appViewModel = AppViewModel()
    @State private var pomodoroViewModel = PomodoroViewModel.shared

    var body: some Scene {
        WindowGroup {
            ContentView()
                .environment(appViewModel)
                .environment(pomodoroViewModel)
                .environment(\.appContainer, .shared)
                .preferredColorScheme(appViewModel.colorScheme)
                .task {
                    // Lock on launch if biometric lock is enabled
                    if AppPreferences.shared.biometricLockEnabled {
                        appViewModel.lock()
                    }
                    // Schedule background sync (mirrors BootReceiver on Android)
                    SyncScheduler.shared.scheduleIfNeeded()
                    // Request notification permission and perform initial sync
                    await NotificationScheduler.shared.requestAuthorization()
                    await AppContainer.shared.syncAll()
                    let events = AppContainer.shared.eventRepository.events
                    await NotificationScheduler.shared.rescheduleAll(events: events)
                }
                .onOpenURL { url in
                    handleDeepLink(url)
                }
        }
        .windowStyle(.titleBar)
        .windowToolbarStyle(.unified)
        .commands {
            CommandGroup(replacing: .newItem) {
                Button("New Task") {
                    appViewModel.triggerNewTask()
                }
                .keyboardShortcut("n", modifiers: .command)
            }
            CommandGroup(after: .newItem) {
                Button("Sync Now") {
                    Task {
                        await AppContainer.shared.syncAll()
                        let events = AppContainer.shared.eventRepository.events
                        await NotificationScheduler.shared.rescheduleAll(events: events)
                    }
                }
                .keyboardShortcut("r", modifiers: .command)
            }
        }

        Settings {
            SettingsView()
                .environment(appViewModel)
                .environment(\.appContainer, .shared)
        }
    }

    private func handleDeepLink(_ url: URL) {
        guard url.scheme == "crossdashboard" else { return }
        if url.host == "tasks",
           let components = URLComponents(url: url, resolvingAgainstBaseURL: false),
           components.queryItems?.first(where: { $0.name == "action" })?.value == "add" {
            appViewModel.triggerNewTask()
        }
    }
}
