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
@MainActor
final class SyncScheduler {

    static let shared = SyncScheduler()

    private let activityIdentifier = "com.crossdashboard.backgroundsync"
    private var activity: NSBackgroundActivityScheduler?

    private init() {}

    // ─── Public API ───────────────────────────────────────────────────────────

    /// Creates or replaces the background activity if it is not already running.
    func scheduleIfNeeded() {
        BackgroundServiceController.shared.refreshStatus()
        if BackgroundServiceController.shared.isEnabled {
            activity?.invalidate()
            activity = nil
            BackgroundServiceController.shared.reloadSchedule()
            return
        }
        guard activity == nil else { return }
        let scheduler = NSBackgroundActivityScheduler(identifier: activityIdentifier)
        scheduler.repeats = true
        scheduler.interval = syncInterval()
        scheduler.tolerance = scheduler.interval * 0.1
        scheduler.qualityOfService = .utility
        // The NSBackgroundActivityScheduler callback fires on a background thread,
        // so hop to the main actor before touching any @MainActor-isolated singletons.
        scheduler.schedule { completion in
            Task { @MainActor in
                await AppContainer.shared.syncAll()
                let events = AppContainer.shared.eventRepository.events
                await NotificationScheduler.shared.rescheduleAll(events: events)
                completion(.finished)
            }
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

    private func syncInterval() -> TimeInterval {
        Double(AppPreferences.shared.syncIntervalMinutes) * 60
    }
}
