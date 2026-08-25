import CrossDashboardKit
import Foundation
import OSLog
import SwiftData
import WidgetKit

@MainActor
final class AgentSyncCoordinator {
    static let shared = AgentSyncCoordinator()

    private static let logger = Logger(
        subsystem: "com.crossdashboard.app.background-agent",
        category: "Sync"
    )

    private let preferences = AppPreferences.shared
    private let keychain = KeychainStore.shared
    private let eventRepository: EventRepository
    private let taskRepository: TaskRepository
    private let noteRepository: NoteRepository
    private let issueRepository: IssueRepository
    private let memoRepository: MemoRepository
    private var timer: Timer?
    private var syncInProgress = false
    private var lastAttemptDate: Date?

    private init() {
        let context = PersistenceController.shared.container.mainContext
        let statsRepository = StatsRepository(context: context)
        eventRepository = EventRepository(
            context: context,
            client: CalDavClient(keychain: keychain),
            keychain: keychain
        )
        taskRepository = TaskRepository(
            context: context,
            client: CalDavClient(keychain: keychain),
            statsRepo: statsRepository
        )
        noteRepository = NoteRepository(
            context: context,
            client: CalDavClient(keychain: keychain)
        )
        issueRepository = IssueRepository(
            context: context,
            client: GiteaClient(keychain: keychain),
            statsRepo: statsRepository
        )
        memoRepository = MemoRepository(
            context: context,
            client: MemosClient(keychain: keychain)
        )
        reloadSchedule()
    }

    func reloadSchedule() {
        preferences.refreshFromStore()
        timer?.invalidate()
        timer = Timer.scheduledTimer(withTimeInterval: 60, repeats: true) { _ in
            Task { @MainActor in
                await AgentSyncCoordinator.shared.syncIfDue()
            }
        }
        if let timer {
            RunLoop.main.add(timer, forMode: .common)
        }
        Task { await syncIfDue() }
    }

    func syncIfDue() async {
        let reference = maxDate(preferences.lastSyncDate, lastAttemptDate)
        let interval = TimeInterval(max(1, preferences.syncIntervalMinutes) * 60)
        guard reference.map({ Date().timeIntervalSince($0) >= interval }) ?? true else { return }
        _ = await syncNow()
    }

    func syncNow() async -> BackgroundSyncReport {
        guard !syncInProgress else {
            return BackgroundSyncReport(succeeded: false, failedSources: ["sync already running"])
        }
        syncInProgress = true
        preferences.refreshFromStore()
        lastAttemptDate = Date()
        defer { syncInProgress = false }

        Self.logger.info("Background sync started")
        let calendarHrefs = selectedCalendarHrefs()
        let repositories = giteaRepositories()
        var failures: [String] = []

        if !(await eventRepository.sync(calendarHrefs: calendarHrefs)) { failures.append("events") }
        if !(await taskRepository.sync(calendarHrefs: calendarHrefs)) { failures.append("tasks") }
        if !(await noteRepository.sync(calendarHrefs: calendarHrefs)) { failures.append("notes") }
        if !(await issueRepository.sync(repositories: repositories)) { failures.append("issues") }
        if !(await memoRepository.syncMemos()) { failures.append("capture") }

        eventRepository.loadFromDB()
        taskRepository.loadFromDB()
        noteRepository.loadFromDB()
        issueRepository.loadFromDB()
        memoRepository.loadFromDB()

        writeWidgetSnapshot()
        WidgetCenter.shared.reloadAllTimelines()
        await AgentNotificationScheduler.shared.rescheduleAll(events: eventRepository.events)
        await AgentDesktopBackgroundRefresher.shared.refreshIfEnabled(
            events: eventRepository.events,
            tasks: taskRepository.allTasks,
            issues: issueRepository.allIssues
        )

        let succeeded = failures.isEmpty
        if succeeded {
            preferences.lastSyncDate = Date()
            Self.logger.info("Background sync completed")
        } else {
            Self.logger.error("Background sync retained cache after failures: \(failures.joined(separator: ", "), privacy: .public)")
        }

        DistributedNotificationCenter.default().postNotificationName(
            Notification.Name(BackgroundServiceContract.syncDidCompleteNotification),
            object: nil,
            userInfo: nil,
            deliverImmediately: true
        )
        return BackgroundSyncReport(succeeded: succeeded, failedSources: failures)
    }

    private func writeWidgetSnapshot() {
        let now = Date()
        let weekFromNow = Calendar.current.date(byAdding: .day, value: 7, to: now) ?? now
        let events = eventRepository.events
            .filter { $0.start >= now && $0.start <= weekFromNow }
            .sorted { $0.start < $1.start }
            .prefix(5)
            .map {
                WidgetUpcomingEvent(
                    id: $0.uid,
                    summary: $0.summary,
                    startEpoch: $0.start.timeIntervalSince1970,
                    calendarColor: nil
                )
            }
        let tasks = taskRepository.allTasks
            .filter {
                $0.status != .completed &&
                $0.status != .cancelled &&
                $0.due.map { $0 <= weekFromNow } == true
            }
            .sorted { ($0.due ?? .distantFuture) < ($1.due ?? .distantFuture) }
            .prefix(5)
            .map {
                WidgetDueTask(
                    id: $0.uid,
                    summary: $0.summary,
                    dueEpoch: $0.due?.timeIntervalSince1970,
                    isOverdue: ($0.due ?? .distantFuture) < now,
                    priority: $0.priority
                )
            }
        WidgetDataStore.save(
            WidgetSnapshot(
                upcomingEvents: Array(events),
                dueSoonTasks: Array(tasks),
                openIssuesCount: issueRepository.openIssues.count,
                syncIntervalMinutes: preferences.syncIntervalMinutes
            )
        )
    }

    private func selectedCalendarHrefs() -> [String] {
        guard let raw = keychain.get(CredentialKey.caldavSelectedCalendars) else { return [] }
        return (try? JSONDecoder().decode([CalDavCalendar].self, from: Data(raw.utf8)))?.map(\.href) ?? []
    }

    private func giteaRepositories() -> [String] {
        guard let raw = keychain.get(CredentialKey.giteaRepos) else { return [] }
        if let decoded = try? JSONDecoder().decode([String].self, from: Data(raw.utf8)) {
            return decoded
        }
        return raw.split(separator: ",").map(String.init).filter { !$0.isEmpty }
    }

    private func maxDate(_ lhs: Date?, _ rhs: Date?) -> Date? {
        switch (lhs, rhs) {
        case let (lhs?, rhs?): max(lhs, rhs)
        case let (lhs?, nil): lhs
        case let (nil, rhs?): rhs
        case (nil, nil): nil
        }
    }
}
