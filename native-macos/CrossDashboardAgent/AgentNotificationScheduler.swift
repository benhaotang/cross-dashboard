import CrossDashboardKit
import CryptoKit
import Foundation
import UserNotifications

@MainActor
final class AgentNotificationScheduler {
    static let shared = AgentNotificationScheduler()

    private let center = UNUserNotificationCenter.current()

    func rescheduleAll(events: [CalendarEvent]) async {
        guard AppPreferences.shared.notificationsEnabled else { return }
        _ = try? await center.requestAuthorization(options: [.alert, .sound, .badge])
        let pending = await center.pendingNotificationRequests()
        center.removePendingNotificationRequests(
            withIdentifiers: pending.map(\.identifier).filter { $0.hasPrefix("event-") }
        )
        let now = Date()
        let minutesBefore = AppPreferences.shared.notificationMinutesBefore
        for event in events.filter({ $0.start > now }).sorted(by: { $0.start < $1.start }).prefix(64) {
            let fireDate = event.start.addingTimeInterval(-Double(minutesBefore) * 60)
            guard fireDate > now else { continue }
            let content = UNMutableNotificationContent()
            content.title = event.summary
            content.body = event.start.formatted(date: .omitted, time: .shortened)
            content.sound = .default
            content.userInfo = ["eventUID": event.uid]
            var components = Calendar.current.dateComponents(
                [.year, .month, .day, .hour, .minute],
                from: fireDate
            )
            components.second = 0
            let request = UNNotificationRequest(
                identifier: stableID(for: event.uid),
                content: content,
                trigger: UNCalendarNotificationTrigger(dateMatching: components, repeats: false)
            )
            try? await center.add(request)
        }
    }

    private func stableID(for uid: String) -> String {
        let digest = SHA256.hash(data: Data(uid.utf8))
        let value = digest.prefix(4).reduce(0) { ($0 << 8) | Int($1) }
        return "event-\(abs(value) % 100_000)"
    }
}
