import Foundation
import Observation

/// Non-sensitive preferences backed by UserDefaults.
/// Mirrors AppPreferences.kt (DataStore) on Android.
@Observable
@MainActor
public final class AppPreferences {
    public static let shared = AppPreferences()

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

    private init(defaults: UserDefaults = .standard) {
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
    }
}

private extension Int {
    var nonZero: Int? { self == 0 ? nil : self }
}
