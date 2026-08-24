import Foundation
import Observation

/// Non-sensitive preferences backed by UserDefaults.
/// Mirrors AppPreferences.kt (DataStore) on Android.
@Observable
@MainActor
public final class AppPreferences {
    /// Team-ID app groups are authorized directly by macOS and do not depend on a provisioning
    /// profile containing a separately registered `group.*` identifier.
    public nonisolated static let appGroupSuiteName = "569WLL4Q5F.com.crossdashboard"
    private nonisolated static let legacyAppGroupSuiteName = "group.com.crossdashboard"

    public static let shared: AppPreferences = {
        let sharedDefaults = UserDefaults(suiteName: appGroupSuiteName) ?? .standard
        if let legacyAppGroup = UserDefaults(suiteName: legacyAppGroupSuiteName) {
            migrateLegacyDefaults(
                from: legacyAppGroup,
                to: sharedDefaults,
                migrationKey: teamAppGroupMigrationKey
            )
        }
        migrateLegacyDefaults(from: .standard, to: sharedDefaults)
        return AppPreferences(defaults: sharedDefaults)
    }()

    private let defaults: UserDefaults

    // ─── Theme ───────────────────────────────────────────────────────────────

    public var theme: ThemePreference {
        didSet { defaults.set(theme.rawValue, forKey: Keys.theme) }
    }

    /// Nil means automatic: use the operating-system timezone.
    public var timeZoneOverride: String? {
        didSet {
            if let value = timeZoneOverride, !value.isEmpty {
                defaults.set(value, forKey: Keys.timeZoneOverride)
            } else {
                defaults.removeObject(forKey: Keys.timeZoneOverride)
            }
            AppTimeZone.applyOverride(timeZoneOverride)
        }
    }

    // ─── Visible screens ─────────────────────────────────────────────────────

    public var visibleScreens: [String] {
        didSet { defaults.set(visibleScreens.joined(separator: ","), forKey: Keys.visibleScreens) }
    }

    // ─── Kanban columns ──────────────────────────────────────────────────────

    public var kanbanColumns: [String] {
        didSet { defaults.set(kanbanColumns.joined(separator: ","), forKey: Keys.kanbanColumns) }
    }

    // ─── Pomodoro ────────────────────────────────────────────────────────────

    public var pomodoroSettings: PomodoroSettings {
        didSet {
            defaults.set(pomodoroSettings.workMinutes,            forKey: Keys.pomodoroWork)
            defaults.set(pomodoroSettings.shortBreakMinutes,      forKey: Keys.pomodoroShort)
            defaults.set(pomodoroSettings.longBreakMinutes,       forKey: Keys.pomodoroLong)
            defaults.set(pomodoroSettings.sessionsUntilLongBreak, forKey: Keys.pomodoroSessions)
        }
    }

    // ─── Task input defaults ─────────────────────────────────────────────────

    public var taskDefaults: TaskDefaults {
        didSet {
            defaults.set(taskDefaults.morningHour,   forKey: Keys.taskMorningHour)
            defaults.set(taskDefaults.afternoonHour, forKey: Keys.taskAfternoonHour)
            defaults.set(taskDefaults.nightHour,     forKey: Keys.taskNightHour)
            defaults.set(taskDefaults.defaultHour,   forKey: Keys.taskDefaultHour)
        }
    }

    // ─── Notifications ───────────────────────────────────────────────────────

    public var notificationsEnabled: Bool {
        didSet { defaults.set(notificationsEnabled, forKey: Keys.notifEnabled) }
    }

    public var notificationMinutesBefore: Int {
        didSet { defaults.set(notificationMinutesBefore, forKey: Keys.notifMinutes) }
    }

    // ─── Sync interval ───────────────────────────────────────────────────────

    /// Background sync interval in minutes. Default is 60.
    public var syncIntervalMinutes: Int {
        didSet { defaults.set(syncIntervalMinutes, forKey: Keys.syncInterval) }
    }

    // ─── Biometric lock ──────────────────────────────────────────────────────

    public var biometricLockEnabled: Bool {
        didSet { defaults.set(biometricLockEnabled, forKey: Keys.biometricLock) }
    }

    // ─── Pomodoro menu bar ───────────────────────────────────────────────────

    public var showPomodoroInMenuBar: Bool {
        didSet { defaults.set(showPomodoroInMenuBar, forKey: Keys.pomodoroMenuBar) }
    }

    // ─── Last sync ───────────────────────────────────────────────────────────

    public var lastSyncDate: Date? {
        didSet {
            if let d = lastSyncDate {
                defaults.set(d.timeIntervalSince1970, forKey: Keys.lastSync)
            } else {
                defaults.removeObject(forKey: Keys.lastSync)
            }
        }
    }

    // ─── Init ─────────────────────────────────────────────────────────────────

    init(defaults: UserDefaults, legacyDefaults: UserDefaults? = nil) {
        if let legacyDefaults {
            Self.migrateLegacyDefaults(from: legacyDefaults, to: defaults)
        }
        self.defaults = defaults

        let rawTheme = defaults.string(forKey: Keys.theme) ?? ""
        theme = ThemePreference(rawValue: rawTheme) ?? .system
        timeZoneOverride = defaults.string(forKey: Keys.timeZoneOverride)

        let screensRaw = defaults.string(forKey: Keys.visibleScreens)
        visibleScreens = screensRaw.flatMap {
            let parts = $0.split(separator: ",").map(String.init).filter { !$0.isEmpty }
            return parts.isEmpty ? nil : parts
        } ?? allScreens

        let columnsRaw = defaults.string(forKey: Keys.kanbanColumns)
        kanbanColumns = columnsRaw.flatMap {
            let parts = $0.split(separator: ",").map(String.init).filter { !$0.isEmpty }
            return parts.isEmpty ? nil : parts
        } ?? defaultKanbanColumns

        pomodoroSettings = PomodoroSettings(
            workMinutes:            defaults.integer(forKey: Keys.pomodoroWork).nonZero ?? 25,
            shortBreakMinutes:      defaults.integer(forKey: Keys.pomodoroShort).nonZero ?? 5,
            longBreakMinutes:       defaults.integer(forKey: Keys.pomodoroLong).nonZero ?? 15,
            sessionsUntilLongBreak: defaults.integer(forKey: Keys.pomodoroSessions).nonZero ?? 4
        )

        taskDefaults = TaskDefaults(
            morningHour:   defaults.integer(forKey: Keys.taskMorningHour).nonZero ?? 8,
            afternoonHour: defaults.integer(forKey: Keys.taskAfternoonHour).nonZero ?? 13,
            nightHour:     defaults.integer(forKey: Keys.taskNightHour).nonZero ?? 21,
            defaultHour:   defaults.integer(forKey: Keys.taskDefaultHour).nonZero ?? 10
        )

        notificationsEnabled      = defaults.object(forKey: Keys.notifEnabled) as? Bool ?? true
        notificationMinutesBefore = defaults.integer(forKey: Keys.notifMinutes).nonZero ?? 15
        syncIntervalMinutes       = defaults.integer(forKey: Keys.syncInterval).nonZero ?? 60
        biometricLockEnabled      = defaults.bool(forKey: Keys.biometricLock)
        showPomodoroInMenuBar    = defaults.object(forKey: Keys.pomodoroMenuBar) as? Bool ?? true

        let epoch = defaults.double(forKey: Keys.lastSync)
        lastSyncDate = epoch > 0 ? Date(timeIntervalSince1970: epoch) : nil
        AppTimeZone.applyOverride(timeZoneOverride)
    }

    // ─── Keys ─────────────────────────────────────────────────────────────────

    private enum Keys {
        static let theme               = "pref_theme"
        static let timeZoneOverride    = "pref_time_zone_override"
        static let visibleScreens      = "pref_visible_screens"
        static let kanbanColumns       = "pref_kanban_columns"
        static let pomodoroWork        = "pref_pomodoro_work"
        static let pomodoroShort       = "pref_pomodoro_short"
        static let pomodoroLong        = "pref_pomodoro_long"
        static let pomodoroSessions    = "pref_pomodoro_sessions"
        static let taskMorningHour     = "pref_task_morning_hour"
        static let taskAfternoonHour   = "pref_task_afternoon_hour"
        static let taskNightHour       = "pref_task_night_hour"
        static let taskDefaultHour     = "pref_task_default_hour"
        static let notifEnabled        = "pref_notif_enabled"
        static let notifMinutes        = "pref_notif_minutes"
        static let syncInterval        = "pref_sync_interval"
        static let biometricLock       = "pref_biometric_lock"
        static let pomodoroMenuBar     = "pref_pomodoro_menu_bar"
        static let lastSync            = "pref_last_sync"

        static let all = [
            theme, timeZoneOverride, visibleScreens, kanbanColumns,
            pomodoroWork, pomodoroShort, pomodoroLong, pomodoroSessions,
            taskMorningHour, taskAfternoonHour, taskNightHour, taskDefaultHour,
            notifEnabled, notifMinutes, syncInterval, biometricLock,
            pomodoroMenuBar, lastSync,
        ]
    }

    private nonisolated static let appGroupMigrationKey = "migration_app_group_defaults_v1"
    private nonisolated static let teamAppGroupMigrationKey = "migration_team_app_group_defaults_v2"

    // DesktopBackgroundManager belongs to the app target, but its persisted settings must move in
    // the same one-time migration so the future agent can read them from the shared suite.
    private static let desktopBackgroundKeys = [
        "desktop_background_definition",
        "desktop_background_image_path",
        "desktop_background_light_image_path",
        "desktop_background_dark_image_path",
        "desktop_background_glass_opacity",
        "desktop_background_image_fit",
    ]

    private static func migrateLegacyDefaults(
        from legacy: UserDefaults,
        to shared: UserDefaults,
        migrationKey: String = appGroupMigrationKey
    ) {
        guard !shared.bool(forKey: migrationKey) else { return }
        for key in Keys.all + desktopBackgroundKeys where shared.object(forKey: key) == nil {
            if let value = legacy.object(forKey: key) {
                shared.set(value, forKey: key)
            }
        }
        shared.set(true, forKey: migrationKey)
    }
}

private extension Int {
    var nonZero: Int? { self == 0 ? nil : self }
}
