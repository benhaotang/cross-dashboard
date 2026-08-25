import SwiftUI
import CrossDashboardKit

@main
struct CrossDashboardApp: App {

    @NSApplicationDelegateAdaptor(AppDelegate.self) private var appDelegate

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
                // Prefer routing crossdashboard:// URLs to the existing window rather than
                // opening a new one. `preferring` reuses any open window; `allowing` permits
                // a new window only when the app is not yet running.
                .handlesExternalEvents(preferring: Set(["crossdashboard"]), allowing: Set(["crossdashboard"]))
        }
        .handlesExternalEvents(matching: Set(["*"]))
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
        let components = URLComponents(url: url, resolvingAgainstBaseURL: false)
        switch url.host {
        case "tasks":
            if components?.queryItems?.first(where: { $0.name == "action" })?.value == "add" {
                appViewModel.triggerNewTask()
            }
        case "capture":
            // crossdashboard://capture?text=<percent-encoded text>
            let text = components?.queryItems?.first(where: { $0.name == "text" })?.value ?? ""
            appViewModel.triggerCapture(text: text)
        case "timer", "pomodoro":
            if let request = TimerDeepLinkRequest(url: url) {
                handleTimerDeepLink(request)
            }
        default:
            break
        }
    }

    private func handleTimerDeepLink(_ request: TimerDeepLinkRequest) {
        switch request.action {
        case .pick:
            appViewModel.presentPomodoroPicker(initialName: request.name ?? "")
        case .start:
            startTimer(from: request)
        case .pause:
            pomodoroViewModel.pause()
        case .resume:
            pomodoroViewModel.resume()
        case .toggle:
            if !pomodoroViewModel.state.active {
                appViewModel.presentPomodoroPicker()
            } else if pomodoroViewModel.state.running {
                pomodoroViewModel.pause()
            } else {
                pomodoroViewModel.resume()
            }
        case .stop:
            pomodoroViewModel.stop()
        case .skip:
            if pomodoroViewModel.state.active {
                pomodoroViewModel.skip()
            }
        }
    }

    private func startTimer(from request: TimerDeepLinkRequest) {
        guard !pomodoroViewModel.state.active else {
            appViewModel.presentPomodoroPicker()
            return
        }

        switch request.targetType {
        case .timer:
            guard let name = request.name else {
                appViewModel.presentPomodoroPicker()
                return
            }
            pomodoroViewModel.startNamed(name, minutes: request.minutes)
        case .task:
            guard let task = matchingTask(id: request.targetID, name: request.name) else {
                appViewModel.presentPomodoroPicker(initialName: request.name ?? "")
                return
            }
            pomodoroViewModel.start(for: task, minutes: request.minutes)
        case .issue:
            guard let issue = matchingIssue(id: request.targetID, name: request.name) else {
                appViewModel.presentPomodoroPicker(initialName: request.name ?? "")
                return
            }
            pomodoroViewModel.startForIssue(
                title: issue.title,
                id: String(issue.id),
                minutes: request.minutes
            )
        }
    }

    private func matchingTask(id: String?, name: String?) -> CalDavTask? {
        let tasks = AppContainer.shared.taskRepository.activeTasks
        if let id, let task = tasks.first(where: { $0.uid == id }) {
            return task
        }
        guard let name else { return nil }
        return tasks.first { $0.summary.caseInsensitiveCompare(name) == .orderedSame }
    }

    private func matchingIssue(id: String?, name: String?) -> GiteaIssue? {
        let issues = AppContainer.shared.issueRepository.openIssues
        if let id, let issue = issues.first(where: {
            String($0.id) == id || "\($0.repository)#\($0.number)" == id
        }) {
            return issue
        }
        guard let name else { return nil }
        return issues.first { $0.title.caseInsensitiveCompare(name) == .orderedSame }
    }
}
