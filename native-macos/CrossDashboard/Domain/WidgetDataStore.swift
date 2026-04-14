import Foundation

// ─── Payload types ────────────────────────────────────────────────────────────
// Lightweight, Codable structs written by the main app and read by the widget.
// Kept separate from the full domain models to avoid any SwiftData dependency
// in the widget process.

public struct WidgetUpcomingEvent: Codable, Sendable, Identifiable {
    public let id: String
    public let summary: String
    public let startEpoch: Double
    public let calendarColor: String?

    public init(id: String, summary: String, startEpoch: Double, calendarColor: String?) {
        self.id = id
        self.summary = summary
        self.startEpoch = startEpoch
        self.calendarColor = calendarColor
    }
}

public struct WidgetDueTask: Codable, Sendable, Identifiable {
    public let id: String
    public let summary: String
    public let dueEpoch: Double?
    public let isOverdue: Bool
    public let priority: Int

    public init(id: String, summary: String, dueEpoch: Double?, isOverdue: Bool, priority: Int) {
        self.id = id
        self.summary = summary
        self.dueEpoch = dueEpoch
        self.isOverdue = isOverdue
        self.priority = priority
    }
}

public struct WidgetSnapshot: Codable, Sendable {
    public let upcomingEvents: [WidgetUpcomingEvent]
    public let dueSoonTasks: [WidgetDueTask]
    public let openIssuesCount: Int
    public let writtenAt: Double        // Date.timeIntervalSince1970
    /// Mirrors AppPreferences.syncIntervalMinutes so the widget can schedule its
    /// next timeline refresh at the same cadence the user configured.
    public let syncIntervalMinutes: Int

    public init(
        upcomingEvents: [WidgetUpcomingEvent],
        dueSoonTasks: [WidgetDueTask],
        openIssuesCount: Int,
        syncIntervalMinutes: Int = 60,
        writtenAt: Double = Date().timeIntervalSince1970
    ) {
        self.upcomingEvents = upcomingEvents
        self.dueSoonTasks = dueSoonTasks
        self.openIssuesCount = openIssuesCount
        self.syncIntervalMinutes = syncIntervalMinutes
        self.writtenAt = writtenAt
    }
}

// ─── Store ────────────────────────────────────────────────────────────────────

/// Writes / reads the widget snapshot via App Group UserDefaults.
/// Both the main app and the widget extension share `group.com.crossdashboard`.
public enum WidgetDataStore {

    private static let suiteName = "group.com.crossdashboard"
    private static let key       = "com.crossdashboard.widgetSnapshot"

    /// Called by the main app after every sync.
    public static func save(_ snapshot: WidgetSnapshot) {
        guard
            let defaults = UserDefaults(suiteName: suiteName),
            let data     = try? JSONEncoder().encode(snapshot)
        else { return }
        defaults.set(data, forKey: key)
    }

    /// Called by the widget's `TimelineProvider`.
    /// Returns `nil` if no snapshot has been written yet.
    public static func load() -> WidgetSnapshot? {
        guard
            let defaults = UserDefaults(suiteName: suiteName),
            let data     = defaults.data(forKey: key)
        else { return nil }
        return try? JSONDecoder().decode(WidgetSnapshot.self, from: data)
    }
}
