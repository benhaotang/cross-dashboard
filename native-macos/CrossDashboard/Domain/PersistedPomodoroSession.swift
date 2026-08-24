import Foundation

public enum PomodoroTargetKind: String, Codable, Sendable {
    case none
    case task
    case issue
}

/// The service-owned Pomodoro state written to the App Group after every transition.
/// A missing record means no Pomodoro is active.
public struct PersistedPomodoroSession: Codable, Equatable, Sendable {
    public var phase: PomodoroPhase
    public var targetKind: PomodoroTargetKind
    public var targetID: String
    public var title: String
    public var phaseStartedAt: Date
    public var deadline: Date?
    public var isPaused: Bool
    public var pausedSecondsRemaining: Int?
    public var completedSessions: Int
    public var settings: PomodoroSettings

    public init(
        phase: PomodoroPhase,
        targetKind: PomodoroTargetKind = .none,
        targetID: String = "",
        title: String = "",
        phaseStartedAt: Date,
        deadline: Date?,
        isPaused: Bool,
        pausedSecondsRemaining: Int? = nil,
        completedSessions: Int = 0,
        settings: PomodoroSettings
    ) {
        self.phase = phase
        self.targetKind = targetKind
        self.targetID = targetID
        self.title = title
        self.phaseStartedAt = phaseStartedAt
        self.deadline = deadline
        self.isPaused = isPaused
        self.pausedSecondsRemaining = pausedSecondsRemaining
        self.completedSessions = completedSessions
        self.settings = settings
    }

    public func secondsRemaining(at date: Date) -> Int {
        if isPaused {
            return max(0, pausedSecondsRemaining ?? 0)
        }
        guard let deadline else { return 0 }
        return max(0, Int(deadline.timeIntervalSince(date).rounded(.up)))
    }
}

public enum PomodoroSessionStore {
    public static let key = "com.crossdashboard.pomodoroSession"

    public static func load(
        defaults: UserDefaults = UserDefaults(suiteName: AppPreferences.appGroupSuiteName) ?? .standard
    ) -> PersistedPomodoroSession? {
        guard let data = defaults.data(forKey: key) else { return nil }
        return try? JSONDecoder().decode(PersistedPomodoroSession.self, from: data)
    }

    public static func save(
        _ session: PersistedPomodoroSession,
        defaults: UserDefaults = UserDefaults(suiteName: AppPreferences.appGroupSuiteName) ?? .standard
    ) throws {
        defaults.set(try JSONEncoder().encode(session), forKey: key)
    }

    public static func clear(
        defaults: UserDefaults = UserDefaults(suiteName: AppPreferences.appGroupSuiteName) ?? .standard
    ) {
        defaults.removeObject(forKey: key)
    }
}
