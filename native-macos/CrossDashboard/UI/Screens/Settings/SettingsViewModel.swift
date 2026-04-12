import Foundation
import Observation
import AppKit
import CrossDashboardKit

@Observable
@MainActor
final class SettingsViewModel {

    // ─── CalDAV ───────────────────────────────────────────────────────────────

    var caldavAuthMethod: CalDavAuthMethod = .manual
    var caldavServer: String = ""
    var caldavUsername: String = ""
    var caldavPassword: String = ""
    var caldavLoginFlowStatus: String = ""
    var caldavIsTestingConnection: Bool = false
    var caldavConnectionResult: String?

    // Calendar multi-select
    var availableCalendars: [CalDavCalendar] = []
    var selectedCalendarHrefs: Set<String> = []
    var defaultEventCalendarHref: String = ""
    var defaultTaskCalendarHref: String = ""
    var isLoadingCalendars: Bool = false

    // ─── Gitea ────────────────────────────────────────────────────────────────

    var giteaInstance: String = ""
    var giteaToken: String = ""
    var giteaReposInput: String = ""  // comma-separated

    // ─── Appearance ───────────────────────────────────────────────────────────

    var visibleScreensSet: Set<String> = Set(allScreens)
    var kanbanColumnsInput: String = ""

    // ─── Task defaults ────────────────────────────────────────────────────────

    var taskMorningHour: Int = 8
    var taskAfternoonHour: Int = 13
    var taskNightHour: Int = 21
    var taskDefaultHour: Int = 10

    // ─── Pomodoro ────────────────────────────────────────────────────────────

    var pomodoroWork: Int = 25
    var pomodoroShort: Int = 5
    var pomodoroLong: Int = 15
    var pomodoroSessions: Int = 4
    var showPomodoroInMenuBar: Bool = true

    // ─── Notifications ────────────────────────────────────────────────────────

    var notificationsEnabled: Bool = true
    var notificationMinutesBefore: Int = 15
    var syncIntervalMinutes: Int = 60

    // ─── Security ─────────────────────────────────────────────────────────────

    var biometricLockEnabled: Bool = false

    // ─── Dependencies ─────────────────────────────────────────────────────────

    private let container: AppContainer
    private var loginFlowTask: Task<Void, Never>?

    init(container: AppContainer = .shared) {
        self.container = container
        loadAll()
    }

    // ─── Load from store ──────────────────────────────────────────────────────

    func loadAll() {
        let prefs   = container.preferences
        let keychain = container.keychain

        // CalDAV
        if let raw = keychain.get(CredentialKey.caldavAuthMethod),
           let method = CalDavAuthMethod(rawValue: raw) {
            caldavAuthMethod = method
        }
        caldavServer   = keychain.get(CredentialKey.caldavServer)   ?? ""
        caldavUsername = keychain.get(CredentialKey.caldavUsername)  ?? ""
        caldavPassword = keychain.get(CredentialKey.caldavPassword)  ?? ""

        // Selected calendars
        if let raw = keychain.get(CredentialKey.caldavSelectedCalendars),
           let cals = try? JSONDecoder().decode([CalDavCalendar].self, from: Data(raw.utf8)) {
            availableCalendars     = cals
            selectedCalendarHrefs  = Set(cals.map(\.href))
        }
        defaultEventCalendarHref = keychain.get(CredentialKey.caldavDefaultEventCalendar) ?? ""
        defaultTaskCalendarHref  = keychain.get(CredentialKey.caldavDefaultTaskCalendar)  ?? ""

        // Gitea
        giteaInstance   = keychain.get(CredentialKey.giteaInstance) ?? ""
        giteaToken      = keychain.get(CredentialKey.giteaToken)    ?? ""
        if let raw = keychain.get(CredentialKey.giteaRepos),
           let repos = try? JSONDecoder().decode([String].self, from: Data(raw.utf8)) {
            giteaReposInput = repos.joined(separator: ", ")
        }

        // Appearance
        visibleScreensSet   = Set(prefs.visibleScreens)
        kanbanColumnsInput  = prefs.kanbanColumns.joined(separator: ", ")

        // Task defaults
        taskMorningHour   = prefs.taskDefaults.morningHour
        taskAfternoonHour = prefs.taskDefaults.afternoonHour
        taskNightHour     = prefs.taskDefaults.nightHour
        taskDefaultHour   = prefs.taskDefaults.defaultHour

        // Pomodoro
        pomodoroWork     = prefs.pomodoroSettings.workMinutes
        pomodoroShort    = prefs.pomodoroSettings.shortBreakMinutes
        pomodoroLong     = prefs.pomodoroSettings.longBreakMinutes
        pomodoroSessions = prefs.pomodoroSettings.sessionsUntilLongBreak
        showPomodoroInMenuBar = prefs.showPomodoroInMenuBar

        // Notifications
        notificationsEnabled      = prefs.notificationsEnabled
        notificationMinutesBefore = prefs.notificationMinutesBefore
        syncIntervalMinutes       = prefs.syncIntervalMinutes

        // Security
        biometricLockEnabled = prefs.biometricLockEnabled
    }

    // ─── Save all ─────────────────────────────────────────────────────────────

    func saveCalDav() {
        container.keychain.set(CredentialKey.caldavAuthMethod, value: caldavAuthMethod.rawValue)
        container.keychain.set(CredentialKey.caldavServer,   value: caldavServer)
        container.keychain.set(CredentialKey.caldavUsername, value: caldavUsername)
        if !caldavPassword.isEmpty {
            container.keychain.set(CredentialKey.caldavPassword, value: caldavPassword)
        }
    }

    func saveCalendars() {
        let selected = availableCalendars.filter { selectedCalendarHrefs.contains($0.href) }
        if let data = try? JSONEncoder().encode(selected),
           let str  = String(data: data, encoding: .utf8) {
            container.keychain.set(CredentialKey.caldavSelectedCalendars, value: str)
        }
        container.keychain.set(CredentialKey.caldavDefaultEventCalendar, value: defaultEventCalendarHref)
        container.keychain.set(CredentialKey.caldavDefaultTaskCalendar,  value: defaultTaskCalendarHref)
    }

    func saveGitea() {
        container.keychain.set(CredentialKey.giteaInstance, value: giteaInstance)
        container.keychain.set(CredentialKey.giteaToken,    value: giteaToken)
        let repos = giteaReposInput
            .split(separator: ",")
            .map { $0.trimmingCharacters(in: .whitespaces) }
            .filter { !$0.isEmpty }
        if let data = try? JSONEncoder().encode(repos),
           let str  = String(data: data, encoding: .utf8) {
            container.keychain.set(CredentialKey.giteaRepos, value: str)
        }
    }

    func saveAppearance() {
        container.preferences.visibleScreens = allScreens.filter { visibleScreensSet.contains($0) }
        let cols = kanbanColumnsInput
            .split(separator: ",")
            .map { $0.trimmingCharacters(in: .whitespaces) }
            .filter { !$0.isEmpty }
        if !cols.isEmpty { container.preferences.kanbanColumns = cols }
    }

    func saveTaskDefaults() {
        container.preferences.taskDefaults = TaskDefaults(
            morningHour:   taskMorningHour,
            afternoonHour: taskAfternoonHour,
            nightHour:     taskNightHour,
            defaultHour:   taskDefaultHour
        )
    }

    func savePomodoro() {
        container.preferences.pomodoroSettings = PomodoroSettings(
            workMinutes:            pomodoroWork,
            shortBreakMinutes:      pomodoroShort,
            longBreakMinutes:       pomodoroLong,
            sessionsUntilLongBreak: pomodoroSessions
        )
        container.preferences.showPomodoroInMenuBar = showPomodoroInMenuBar
    }

    func saveNotifications() {
        container.preferences.notificationsEnabled       = notificationsEnabled
        container.preferences.notificationMinutesBefore  = notificationMinutesBefore
        container.preferences.syncIntervalMinutes        = syncIntervalMinutes
        SyncScheduler.shared.reschedule()
    }

    func saveSecurity() {
        container.preferences.biometricLockEnabled = biometricLockEnabled
    }

    // ─── Actions ──────────────────────────────────────────────────────────────

    func testCalDavConnection() {
        saveCalDav()
        caldavIsTestingConnection = true
        caldavConnectionResult = nil
        Task {
            defer { caldavIsTestingConnection = false }
            do {
                let cals = try await container.calDavClient.fetchCalendars()
                availableCalendars = cals
                caldavConnectionResult = "Connected — \(cals.count) calendar(s) found."
            } catch {
                caldavConnectionResult = "Error: \(error.localizedDescription)"
            }
        }
    }

    func startLoginFlowV2() {
        guard !caldavServer.trimmingCharacters(in: .whitespaces).isEmpty else {
            caldavLoginFlowStatus = "Enter a server URL first."
            return
        }
        caldavLoginFlowStatus = "Opening browser…"
        loginFlowTask?.cancel()
        let serverUrl = caldavServer
        loginFlowTask = Task {
            // Step 1: initiate
            let initResult = await container.loginFlow.initiate(serverUrl: serverUrl)
            switch initResult {
            case .failure(let e):
                caldavLoginFlowStatus = "Error: \(e.localizedDescription)"
                return
            case .success(let flowInit):
                if let url = URL(string: flowInit.loginUrl) {
                    await MainActor.run { NSWorkspace.shared.open(url) }
                }
                caldavLoginFlowStatus = "Waiting for browser approval…"

                // Step 2: poll
                let pollResult = await container.loginFlow.poll(
                    pollEndpoint: flowInit.pollEndpoint,
                    pollToken: flowInit.pollToken
                )
                switch pollResult {
                case .failure(let e):
                    caldavLoginFlowStatus = "Login failed: \(e.localizedDescription)"
                case .success(let creds):
                    container.keychain.set(CredentialKey.caldavServer,      value: creds.serverUrl)
                    container.keychain.set(CredentialKey.caldavUsername,     value: creds.loginName)
                    container.keychain.set(CredentialKey.caldavPassword,     value: creds.appPassword)
                    container.keychain.set(CredentialKey.caldavAuthMethod,   value: CalDavAuthMethod.loginFlowV2.rawValue)
                    caldavServer       = creds.serverUrl
                    caldavUsername     = creds.loginName
                    caldavPassword     = creds.appPassword
                    caldavLoginFlowStatus = "Login successful!"
                }
            }
        }
    }

    func syncNow() async {
        await container.syncAll()
    }

    // ─── App version ──────────────────────────────────────────────────────────

    var appVersion: String {
        let version = Bundle.main.infoDictionary?["CFBundleShortVersionString"] as? String ?? "1.0"
        let build   = Bundle.main.infoDictionary?["CFBundleVersion"] as? String ?? "1"
        return "\(version) (\(build))"
    }

    var lastSyncLabel: String {
        guard let date = container.preferences.lastSyncDate else { return "Never" }
        return date.formatted(date: .abbreviated, time: .shortened)
    }
}
