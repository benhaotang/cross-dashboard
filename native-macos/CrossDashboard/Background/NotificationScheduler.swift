import CryptoKit
import Foundation
import UserNotifications
import SwiftUI
import CrossDashboardKit

/// Wraps `UNUserNotificationCenter` to schedule calendar event reminders.
/// Mirrors `EventAlarmScheduler.kt` + `UNUserNotificationCenterDelegate` from Android.
///
/// - Stable notification IDs are derived from `"event-\(uid.hashValue)"` to allow
///   clean cancellation/replacement after each sync.
/// - Caps at 64 scheduled notifications (macOS system limit).
@MainActor
final class NotificationScheduler: NSObject {

    static let shared = NotificationScheduler()

    private let center = UNUserNotificationCenter.current()
    private let maxScheduled = 64

    override private init() {
        super.init()
        center.delegate = self
    }

    // ─── Public API ───────────────────────────────────────────────────────────

    /// Requests `alert` + `sound` authorization on first call. No-op if already granted.
    func requestAuthorization() async {
        _ = try? await center.requestAuthorization(options: [.alert, .sound, .badge])
    }

    /// Cancels all pending event notifications then re-schedules up to `maxScheduled`
    /// future events. Mirrors `EventAlarmScheduler.rescheduleAll()` on Android.
    func rescheduleAll(events: [CalendarEvent]) async {
        // Remove all existing event notifications
        let pending = await center.pendingNotificationRequests()
        let ids = pending.map(\.identifier).filter { $0.hasPrefix("event-") }
        center.removePendingNotificationRequests(withIdentifiers: ids)

        let minutesBefore = AppPreferences.shared.notificationMinutesBefore
        let now = Date()

        let upcoming = events
            .filter { $0.start > now }
            .sorted { $0.start < $1.start }
            .prefix(maxScheduled)

        for event in upcoming {
            let fireDate = event.start.addingTimeInterval(-Double(minutesBefore) * 60)
            guard fireDate > now else { continue }

            let content = UNMutableNotificationContent()
            content.title = event.summary
            content.body  = formattedTimeRange(start: event.start, end: event.end)
            content.sound = .default
            content.userInfo = ["eventUID": event.uid]

            var comps = Calendar.current.dateComponents(
                [.year, .month, .day, .hour, .minute],
                from: fireDate
            )
            comps.second = 0
            let trigger = UNCalendarNotificationTrigger(dateMatching: comps, repeats: false)

            let id = stableID(for: event.uid)
            let request = UNNotificationRequest(identifier: id, content: content, trigger: trigger)
            try? await center.add(request)
        }
    }

    // ─── Helpers ──────────────────────────────────────────────────────────────

    private func stableID(for uid: String) -> String {
        let digest = SHA256.hash(data: Data(uid.utf8))
        let value = digest.prefix(4).reduce(0) { ($0 << 8) | Int($1) }
        return "event-\(abs(value) % 100_000)"
    }

    private func formattedTimeRange(start: Date, end: Date) -> String {
        let fmt = DateFormatter()
        fmt.dateStyle = .none
        fmt.timeStyle = .short
        return "\(fmt.string(from: start)) – \(fmt.string(from: end))"
    }
}

// ─── UNUserNotificationCenterDelegate ────────────────────────────────────────

extension NotificationScheduler: UNUserNotificationCenterDelegate {

    /// Show notifications even when the app is in the foreground.
    nonisolated func userNotificationCenter(
        _ center: UNUserNotificationCenter,
        willPresent notification: UNNotification
    ) async -> UNNotificationPresentationOptions {
        [.banner, .sound]
    }

    /// Handle tap: navigate to the relevant event (post a notification the app can observe).
    nonisolated func userNotificationCenter(
        _ center: UNUserNotificationCenter,
        didReceive response: UNNotificationResponse
    ) async {
        let id = response.notification.request.identifier
        guard id.hasPrefix("event-") else { return }
        let uid = response.notification.request.content.userInfo["eventUID"] as? String
        // Post to main actor so ContentView / AppViewModel can handle navigation.
        await MainActor.run {
            NotificationCenter.default.post(
                name: .crossDashboardOpenEvent,
                object: nil,
                userInfo: uid.map { ["eventUID": $0] } ?? ["notificationID": id]
            )
        }
    }
}

extension Notification.Name {
    static let crossDashboardOpenEvent = Notification.Name("crossDashboardOpenEvent")
}
