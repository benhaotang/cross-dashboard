import Foundation
import UserNotifications
import CrossDashboardKit

/// Wraps `NSBackgroundActivityScheduler` to run periodic background syncs.
/// Mirrors the `SyncWorker` (WorkManager) pattern from the Android app.
///
/// - Schedules itself with a configurable interval (default 60 min).
/// - On fire: calls `AppContainer.shared.syncAll()`, then asks
///   `NotificationScheduler` to reschedule all event alarms.
/// - Call `scheduleIfNeeded()` once from `CrossDashboardApp.init()`.
final class SyncScheduler {

    static let shared = SyncScheduler()

    private let activityIdentifier = "com.crossdashboard.backgroundsync"
    private var activity: NSBackgroundActivityScheduler?

    private init() {}

    // ─── Public API ───────────────────────────────────────────────────────────

    /// Creates or replaces the background activity if it is not already running.
    func scheduleIfNeeded() {
        guard activity == nil else { return }
        let scheduler = NSBackgroundActivityScheduler(identifier: activityIdentifier)
        scheduler.repeats = true
        scheduler.interval = syncInterval()
        scheduler.tolerance = scheduler.interval * 0.1
        scheduler.qualityOfService = .utility
        scheduler.schedule { [weak self] completion in
            self?.performSync(completion: completion)
        }
        activity = scheduler
    }

    /// Invalidates the current schedule and creates a fresh one with the
    /// latest interval from `AppPreferences`. Call when the user changes the
    /// sync interval in Settings.
    func reschedule() {
        activity?.invalidate()
        activity = nil
        scheduleIfNeeded()
    }

    // ─── Private ──────────────────────────────────────────────────────────────

    private func performSync(completion: NSBackgroundActivityScheduler.CompletionHandler) {
        Task {
            await AppContainer.shared.syncAll()
            // After sync, reschedule event notifications.
            let events = AppContainer.shared.eventRepository.events
            await NotificationScheduler.shared.rescheduleAll(events: events)
            completion(.finished)
        }
    }

    private func syncInterval() -> TimeInterval {
        Double(AppPreferences.shared.syncIntervalMinutes) * 60
    }
}
