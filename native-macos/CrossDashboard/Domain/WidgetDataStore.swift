import Foundation
#if canImport(OSLog)
import OSLog
#endif

#if canImport(OSLog)
private let widgetSnapshotLogger = Logger(
    subsystem: Bundle.main.bundleIdentifier ?? "com.crossdashboard",
    category: "WidgetSnapshot"
)
#endif

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

/// Writes and reads the widget snapshot from the App Group container.
/// The main app and widget extension share the Team-ID App Group returned by
/// `AppPreferences.appGroupSuiteName`; UserDefaults remains an upgrade fallback.
public enum WidgetDataStore {

    private static let key       = "com.crossdashboard.widgetSnapshot"
    private static let fileName  = "widget-snapshot.json"

    /// Called by the main app after every sync.
    public static func save(_ snapshot: WidgetSnapshot) {
        let data: Data
        do {
            data = try JSONEncoder().encode(snapshot)
        } catch {
#if canImport(OSLog)
            widgetSnapshotLogger.error("Snapshot encode failed: \(error.localizedDescription, privacy: .public)")
#endif
            return
        }

        if let fileURL {
            do {
                try data.write(to: fileURL, options: .atomic)
#if canImport(OSLog)
                widgetSnapshotLogger.info(
                    "Snapshot saved to \(fileURL.path, privacy: .public), events=\(snapshot.upcomingEvents.count), tasks=\(snapshot.dueSoonTasks.count), issues=\(snapshot.openIssuesCount)"
                )
#endif
            } catch {
#if canImport(OSLog)
                widgetSnapshotLogger.error(
                    "Snapshot file write failed at \(fileURL.path, privacy: .public): \(error.localizedDescription, privacy: .public)"
                )
#endif
            }
        } else {
#if canImport(OSLog)
            widgetSnapshotLogger.error("App Group container URL is unavailable while saving")
#endif
        }

        // Retain the defaults copy as an upgrade fallback for existing installations.
        UserDefaults(suiteName: AppPreferences.appGroupSuiteName)?.set(data, forKey: key)
    }

    /// Called by the widget's `TimelineProvider`.
    /// Returns `nil` if no snapshot has been written yet.
    public static func load() -> WidgetSnapshot? {
        if let fileURL {
            do {
                let data = try Data(contentsOf: fileURL)
                let snapshot = try JSONDecoder().decode(WidgetSnapshot.self, from: data)
#if canImport(OSLog)
                widgetSnapshotLogger.info(
                    "Snapshot loaded from \(fileURL.path, privacy: .public), events=\(snapshot.upcomingEvents.count), tasks=\(snapshot.dueSoonTasks.count), issues=\(snapshot.openIssuesCount)"
                )
#endif
                return snapshot
            } catch {
#if canImport(OSLog)
                widgetSnapshotLogger.error(
                    "Snapshot file load failed at \(fileURL.path, privacy: .public): \(error.localizedDescription, privacy: .public)"
                )
#endif
            }
        } else {
#if canImport(OSLog)
            widgetSnapshotLogger.error("App Group container URL is unavailable while loading")
#endif
        }

        guard let data = UserDefaults(suiteName: AppPreferences.appGroupSuiteName)?.data(forKey: key) else {
#if canImport(OSLog)
            widgetSnapshotLogger.error("Snapshot fallback is absent from App Group UserDefaults")
#endif
            return nil
        }

        do {
            let snapshot = try JSONDecoder().decode(WidgetSnapshot.self, from: data)
#if canImport(OSLog)
            widgetSnapshotLogger.info(
                "Snapshot loaded from UserDefaults fallback, events=\(snapshot.upcomingEvents.count), tasks=\(snapshot.dueSoonTasks.count), issues=\(snapshot.openIssuesCount)"
            )
#endif
            return snapshot
        } catch {
#if canImport(OSLog)
            widgetSnapshotLogger.error("Snapshot fallback decode failed: \(error.localizedDescription, privacy: .public)")
#endif
            return nil
        }
    }

    private static var fileURL: URL? {
#if os(macOS)
        FileManager.default
            .containerURL(forSecurityApplicationGroupIdentifier: AppPreferences.appGroupSuiteName)?
            .appendingPathComponent(fileName, isDirectory: false)
#else
        nil
#endif
    }
}
