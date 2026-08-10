import SwiftUI
import CrossDashboardKit
import UniformTypeIdentifiers

/// Full settings window with tabbed sections.
/// Mirrors SettingsScreen on Android. Opens via Cmd+, (macOS Settings scene).
struct SettingsView: View {

    @State private var viewModel = SettingsViewModel()

    var body: some View {
        TabView {
            CalDavSettingsTab(viewModel: viewModel)
                .tabItem { Label("CalDAV", systemImage: "server.rack") }

            GiteaSettingsTab(viewModel: viewModel)
                .tabItem { Label("Gitea", systemImage: "chevron.left.forwardslash.chevron.right") }

            MemosSettingsTab(viewModel: viewModel)
                .tabItem { Label("Memos", systemImage: "tray.and.arrow.down") }

            AppearanceSettingsTab(viewModel: viewModel)
                .tabItem { Label("Appearance", systemImage: "paintbrush") }

            TaskDefaultsTab(viewModel: viewModel)
                .tabItem { Label("Tasks", systemImage: "checklist") }

            PomodoroSettingsTab(viewModel: viewModel)
                .tabItem { Label("Pomodoro", systemImage: "timer") }

            NotificationsSettingsTab(viewModel: viewModel)
                .tabItem { Label("Notifications", systemImage: "bell") }

            SecuritySettingsTab(viewModel: viewModel)
                .tabItem { Label("Security", systemImage: "lock.shield") }

            AboutSettingsTab(viewModel: viewModel)
                .tabItem { Label("About", systemImage: "info.circle") }
        }
        .frame(minWidth: 520, minHeight: 400)
        .onAppear { viewModel.loadAll() }
    }
}

// ─── CalDAV Tab ───────────────────────────────────────────────────────────────

private struct CalDavSettingsTab: View {
    @Bindable var viewModel: SettingsViewModel

    var body: some View {
        Form {
            Section("Authentication") {
                Picker("Method", selection: $viewModel.caldavAuthMethod) {
                    Text("Login Flow v2").tag(CalDavAuthMethod.loginFlowV2)
                    Text("Manual").tag(CalDavAuthMethod.manual)
                }
                .pickerStyle(.segmented)

                if viewModel.caldavAuthMethod == .loginFlowV2 {
                    TextField("Nextcloud Server URL", text: $viewModel.caldavServer)
                        .textContentType(.URL)
                    Button("Sign in via Browser…") {
                        viewModel.startLoginFlowV2()
                    }
                    .disabled(viewModel.caldavServer.trimmingCharacters(in: .whitespaces).isEmpty)
                    if !viewModel.caldavLoginFlowStatus.isEmpty {
                        Text(viewModel.caldavLoginFlowStatus)
                            .font(.caption)
                            .foregroundStyle(.secondary)
                    }
                } else {
                    TextField("Server URL", text: $viewModel.caldavServer)
                        .textContentType(.URL)
                    TextField("Username", text: $viewModel.caldavUsername)
                        .textContentType(.username)
                    SecureField("Password", text: $viewModel.caldavPassword)
                        .textContentType(.password)
                }
            }

            Section {
                HStack {
                    Button("Save & Test Connection") {
                        viewModel.testCalDavConnection()
                    }
                    .disabled(viewModel.caldavIsTestingConnection)

                    if viewModel.caldavIsTestingConnection {
                        ProgressView().controlSize(.small)
                    }
                }

                if let result = viewModel.caldavConnectionResult {
                    Text(result)
                        .font(.caption)
                        .foregroundStyle(result.hasPrefix("Error") ? .red : .green)
                }
            }

            if !viewModel.availableCalendars.isEmpty {
                Section("Calendars") {
                    ForEach(viewModel.availableCalendars) { cal in
                        Toggle(
                            cal.displayName,
                            isOn: Binding(
                                get: { viewModel.selectedCalendarHrefs.contains(cal.href) },
                                set: { on in
                                    if on { viewModel.selectedCalendarHrefs.insert(cal.href) }
                                    else  { viewModel.selectedCalendarHrefs.remove(cal.href) }
                                }
                            )
                        )
                    }
                }

                Section("Defaults") {
                    Picker("Default event calendar", selection: $viewModel.defaultEventCalendarHref) {
                        Text("None").tag("")
                        ForEach(viewModel.availableCalendars.filter { $0.components.contains("VEVENT") }) { cal in
                            Text(cal.displayName).tag(cal.href)
                        }
                    }
                    Picker("Default task calendar", selection: $viewModel.defaultTaskCalendarHref) {
                        Text("None").tag("")
                        ForEach(viewModel.availableCalendars.filter { $0.components.contains("VTODO") }) { cal in
                            Text(cal.displayName).tag(cal.href)
                        }
                    }
                }

                Section {
                    Button("Save Calendar Settings") {
                        viewModel.saveCalendars()
                    }
                }
            }
        }
        .formStyle(.grouped)
        .padding()
    }
}

// ─── Gitea Tab ────────────────────────────────────────────────────────────────

private struct GiteaSettingsTab: View {
    @Bindable var viewModel: SettingsViewModel

    var body: some View {
        Form {
            Section("Gitea Connection") {
                TextField("Instance URL (e.g. https://git.example.com)", text: $viewModel.giteaInstance)
                    .textContentType(.URL)
                SecureField("Access Token", text: $viewModel.giteaToken)
            }

            Section("Repositories (one per line or comma-separated)") {
                TextEditor(text: $viewModel.giteaReposInput)
                    .frame(minHeight: 80)
                    .font(.system(.body, design: .monospaced))
            }

            Section {
                Button("Save Gitea Settings") {
                    viewModel.saveGitea()
                }
            }
        }
        .formStyle(.grouped)
        .padding()
    }
}

// ─── Appearance Tab ───────────────────────────────────────────────────────────

private struct AppearanceSettingsTab: View {
    @Bindable var viewModel: SettingsViewModel
    @Environment(AppViewModel.self) private var appViewModel

    var body: some View {
        Form {
            Section("Theme") {
                Picker("Color scheme", selection: Binding(
                    get: { appViewModel.theme },
                    set: { appViewModel.theme = $0 }
                )) {
                    ForEach(ThemePreference.allCases, id: \.self) { theme in
                        Text(theme.rawValue.capitalized).tag(theme)
                    }
                }
                .pickerStyle(.segmented)
            }

            DesktopBackgroundSettingsSection()

            Section("Date & time") {
                TextField("Timezone override", text: $viewModel.timeZoneOverride,
                          prompt: Text("Automatic (\(viewModel.systemTimeZone))"))
                    .textFieldStyle(.roundedBorder)
                Text("Leave blank to use the system timezone. Otherwise enter an IANA timezone such as Europe/Berlin.")
                    .font(.caption)
                    .foregroundStyle(.secondary)
                if let error = viewModel.timeZoneError {
                    Text(error).font(.caption).foregroundStyle(.red)
                }
                HStack {
                    Button("Apply") { viewModel.saveTimeZone() }
                    Button("Use System Timezone") {
                        viewModel.timeZoneOverride = ""
                        viewModel.saveTimeZone()
                    }
                }
            }

            Section("Visible screens") {
                Text("Drag to reorder. Toggle to show / hide.")
                    .font(.caption)
                    .foregroundStyle(.secondary)

                // ── Visible (drag to reorder) ──────────────────────────────
                List {
                    ForEach(viewModel.orderedVisibleScreens, id: \.self) { screen in
                        HStack(spacing: 8) {
                            Image(systemName: Screen(rawValue: screen)?.systemImage ?? "circle")
                                .frame(width: 18)
                                .foregroundStyle(.secondary)
                            Text(screen)
                            Spacer()
                            Button {
                                viewModel.toggleVisibleScreen(screen)
                            } label: {
                                Image(systemName: "eye.slash.fill")
                            }
                            .buttonStyle(.plain)
                            .foregroundStyle(.secondary)
                            .accessibilityLabel("Hide \(screen)")
                        }
                        .padding(.vertical, 2)
                    }
                    .onMove { viewModel.moveVisibleScreen(from: $0, to: $1) }
                }
                .frame(height: max(56, CGFloat(viewModel.orderedVisibleScreens.count) * 38))
                .listStyle(.plain)

                // ── Hidden (toggle to restore) ─────────────────────────────
                if !viewModel.hiddenScreens.isEmpty {
                    Divider()
                    Text("Hidden screens")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                    ForEach(viewModel.hiddenScreens, id: \.self) { screen in
                        HStack(spacing: 8) {
                            Image(systemName: Screen(rawValue: screen)?.systemImage ?? "circle")
                                .frame(width: 18)
                                .foregroundStyle(.secondary)
                            Text(screen)
                                .foregroundStyle(.secondary)
                            Spacer()
                            Button {
                                viewModel.toggleVisibleScreen(screen)
                            } label: {
                                Image(systemName: "eye.fill")
                            }
                            .buttonStyle(.plain)
                            .foregroundStyle(.tint)
                            .accessibilityLabel("Show \(screen)")
                        }
                        .padding(.vertical, 2)
                    }
                }
            }

            Section("Kanban columns (comma-separated)") {
                TextField("e.g. backlog, planned, inprogress, done", text: $viewModel.kanbanColumnsInput)
            }

            Section {
                Button("Save Appearance") {
                    viewModel.saveAppearance()
                }
            }
        }
        .formStyle(.grouped)
        .padding()
    }
}

private struct DesktopBackgroundSettingsSection: View {
    @State private var manager = DesktopBackgroundManager.shared
    @State private var choosingPicture = false
    @State private var pendingImageSlot: DesktopBackgroundImageSlot = .both
    @State private var pendingOpacity = 0.8
    var body: some View {
        Section("Desktop background") {
            backdropRow("Both appearances", slot: .both, selected: manager.imagePath != nil)
            backdropRow("Light appearance", slot: .light, selected: manager.lightImagePath != nil)
            backdropRow("Dark appearance", slot: .dark, selected: manager.darkImagePath != nil)
            Text("Light and Dark selections override Both. A multi-image HEIC selected for Both uses its first frame in Light appearance and its last frame in Dark appearance.")
                .font(.caption).foregroundStyle(.secondary)
            Picker("Picture fit", selection: Binding(
                get: { manager.imageFit }, set: { manager.setImageFit($0) }
            )) {
                ForEach(DesktopBackgroundImageFit.allCases) { fit in Text(fit.label).tag(fit) }
            }
            LabeledContent("Glass opacity") {
                HStack {
                    Slider(value: Binding(
                        get: { pendingOpacity }, set: { pendingOpacity = $0 }
                    ), in: 0.5...1, step: 0.05, onEditingChanged: { editing in
                        if !editing { manager.setGlassOpacity(pendingOpacity) }
                    })
                    Text("\(Int(pendingOpacity * 100))%").monospacedDigit().frame(width: 42)
                }.frame(width: 260)
            }
            if let definition = manager.definition {
                LabeledContent("Snapshot", value: definition.summary)
                LabeledContent("Status", value: manager.lastMessage)
                Toggle("Update automatically", isOn: Binding(
                    get: { definition.enabled },
                    set: { $0 ? manager.enable() : manager.disable() }
                ))
                Button("Update now") {
                    Task { await manager.refreshIfEnabled(reason: "Manual Settings update", force: true) }
                }
            } else {
                Text("Use the camera button in Inbox or Views to create a background snapshot.")
                    .foregroundStyle(.secondary)
            }
            Text("Light and dark variants follow the current system appearance. macOS updates the current desktop on each connected display; other Spaces may retain their existing wallpaper.")
                .font(.caption).foregroundStyle(.secondary)
            Text("Generated images are stored in Pictures/Cross-Dashboard/Backgrounds so the macOS desktop service can read them.")
                .font(.caption).foregroundStyle(.secondary)
        }
        .fileImporter(isPresented: $choosingPicture, allowedContentTypes: [.image]) { result in
            if case .success(let url) = result {
                let slot = pendingImageSlot
                Task { await manager.importBackdrop(from: url, for: slot) }
            }
        }
        .onAppear { pendingOpacity = manager.glassOpacity }
    }

    @ViewBuilder
    private func backdropRow(_ label: String, slot: DesktopBackgroundImageSlot, selected: Bool) -> some View {
        LabeledContent(label) {
            HStack {
                Text(manager.imageSummary(for: slot)).foregroundStyle(.secondary)
                Button(selected ? "Replace…" : "Choose…") {
                    pendingImageSlot = slot; choosingPicture = true
                }
                if selected {
                    Button("Remove", role: .destructive) { Task { await manager.removeBackdrop(slot) } }
                }
            }
        }
    }
}

// ─── Task Defaults Tab ────────────────────────────────────────────────────────

private struct TaskDefaultsTab: View {
    @Bindable var viewModel: SettingsViewModel

    var body: some View {
        Form {
            Section("Quick input time-of-day defaults") {
                Stepper("Morning: \(viewModel.taskMorningHour):00", value: $viewModel.taskMorningHour, in: 0...23)
                Stepper("Afternoon: \(viewModel.taskAfternoonHour):00", value: $viewModel.taskAfternoonHour, in: 0...23)
                Stepper("Night: \(viewModel.taskNightHour):00", value: $viewModel.taskNightHour, in: 0...23)
                Stepper("Default: \(viewModel.taskDefaultHour):00", value: $viewModel.taskDefaultHour, in: 0...23)
            }

            Section {
                Button("Save Task Defaults") {
                    viewModel.saveTaskDefaults()
                }
            }
        }
        .formStyle(.grouped)
        .padding()
    }
}

// ─── Pomodoro Tab ─────────────────────────────────────────────────────────────

private struct PomodoroSettingsTab: View {
    @Bindable var viewModel: SettingsViewModel

    var body: some View {
        Form {
            Section("Timer durations") {
                Stepper("Work: \(viewModel.pomodoroWork) min", value: $viewModel.pomodoroWork, in: 1...120)
                Stepper("Short break: \(viewModel.pomodoroShort) min", value: $viewModel.pomodoroShort, in: 1...60)
                Stepper("Long break: \(viewModel.pomodoroLong) min", value: $viewModel.pomodoroLong, in: 1...120)
                Stepper("Sessions until long break: \(viewModel.pomodoroSessions)", value: $viewModel.pomodoroSessions, in: 1...12)
            }

            Section("Menu bar") {
                Toggle("Show Pomodoro in menu bar", isOn: $viewModel.showPomodoroInMenuBar)
            }

            Section {
                Button("Save Pomodoro Settings") {
                    viewModel.savePomodoro()
                }
            }
        }
        .formStyle(.grouped)
        .padding()
    }
}

// ─── Notifications Tab ────────────────────────────────────────────────────────

private struct NotificationsSettingsTab: View {
    @Bindable var viewModel: SettingsViewModel

    var body: some View {
        Form {
            Section("Event reminders") {
                Toggle("Enable notifications", isOn: $viewModel.notificationsEnabled)
                if viewModel.notificationsEnabled {
                    Stepper(
                        "Remind \(viewModel.notificationMinutesBefore) min before",
                        value: $viewModel.notificationMinutesBefore,
                        in: 0...120,
                        step: 5
                    )
                }
            }

            Section("Background sync") {
                Stepper(
                    "Sync every \(viewModel.syncIntervalMinutes) min",
                    value: $viewModel.syncIntervalMinutes,
                    in: 15...480,
                    step: 15
                )
                .accessibilityLabel("Sync interval: \(viewModel.syncIntervalMinutes) minutes")
            }

            Section {
                Button("Save Notification Settings") {
                    viewModel.saveNotifications()
                }
            }
        }
        .formStyle(.grouped)
        .padding()
    }
}

// ─── Memos Tab ────────────────────────────────────────────────────────────────

private struct MemosSettingsTab: View {
    @Bindable var viewModel: SettingsViewModel

    var body: some View {
        Form {
            Section("Connection") {
                TextField("Host URL", text: $viewModel.memosHost, prompt: Text("https://memos.example.com"))
                    .textFieldStyle(.roundedBorder)
                    .accessibilityLabel("Memos host URL")

                SecureField("Access Token", text: $viewModel.memosToken)
                    .textFieldStyle(.roundedBorder)
                    .accessibilityLabel("Memos access token")

                if let msg = viewModel.memosConnectionMessage {
                    Text(msg)
                        .font(.caption)
                        .foregroundStyle(viewModel.memosConnectionSuccess ? Color.green : Color.red)
                }

                HStack(spacing: 12) {
                    Button("Test Connection") {
                        Task { await viewModel.testMemosConnection() }
                    }
                    .disabled(viewModel.memosConnectionTesting)
                    Button("Save") {
                        viewModel.saveMemos()
                    }
                }
            }
        }
        .formStyle(.grouped)
        .padding()
    }
}

// ─── Security Tab ─────────────────────────────────────────────────────────────

private struct SecuritySettingsTab: View {
    @Bindable var viewModel: SettingsViewModel

    var body: some View {
        Form {
            Section("Biometric Lock") {
                Toggle("Lock app with Touch ID", isOn: $viewModel.biometricLockEnabled)
                    .accessibilityLabel("Enable Touch ID lock")

                if viewModel.biometricLockEnabled {
                    Text("The app will require Touch ID (or PIN fallback) on launch.")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }
            }

            Section {
                Button("Save Security Settings") {
                    viewModel.saveSecurity()
                }
            }
        }
        .formStyle(.grouped)
        .padding()
    }
}

// ─── About Tab ────────────────────────────────────────────────────────────────

private struct AboutSettingsTab: View {
    @Bindable var viewModel: SettingsViewModel

    var body: some View {
        Form {
            Section {
                LabeledContent("Version", value: viewModel.appVersion)
                LabeledContent("Last synced", value: viewModel.lastSyncLabel)
            }

            Section {
                Button("Sync Now") {
                    Task { await viewModel.syncNow() }
                }
                .accessibilityLabel("Trigger manual sync")
            }

            Section {
                Link("Source code (GitHub)",
                     destination: URL(string: "https://github.com/your-repo/cross-dashboard")!)
            }
        }
        .formStyle(.grouped)
        .padding()
    }
}
