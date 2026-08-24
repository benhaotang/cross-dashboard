import Foundation
import SwiftData
import WidgetKit
import CrossDashboardKit

/// AppContainer creates and vends all singletons for the app.
/// It is the macOS equivalent of Hilt's singleton scope in the Android app.
/// Inject via `@Environment(\.appContainer)` from SwiftUI views.
@Observable
@MainActor
final class AppContainer {

    // ─── Shared instance ──────────────────────────────────────────────────────

    nonisolated static let shared: AppContainer = MainActor.assumeIsolated { AppContainer() }

    // ─── Data layer ───────────────────────────────────────────────────────────

    let keychain: KeychainStore
    let preferences: AppPreferences
    let persistence: PersistenceController

    let calDavClient: CalDavClient
    let giteaClient: GiteaClient
    let memosClient: MemosClient
    let karakeepClient: KarakeepClient
    let loginFlow: NextcloudLoginFlow

    let statsRepository: StatsRepository
    let eventRepository: EventRepository
    let taskRepository: TaskRepository
    let noteRepository: NoteRepository
    let issueRepository: IssueRepository
    let memoRepository: MemoRepository

    // ─── Init ─────────────────────────────────────────────────────────────────

    private init() {
        keychain     = .shared
        preferences  = .shared
        persistence  = .shared

        calDavClient = CalDavClient(keychain: keychain)
        giteaClient  = GiteaClient(keychain: keychain)
        memosClient  = MemosClient(keychain: keychain)
        karakeepClient = KarakeepClient(keychain: keychain)
        loginFlow    = NextcloudLoginFlow()

        let ctx = persistence.container.mainContext

        statsRepository = StatsRepository(context: ctx)
        eventRepository = EventRepository(context: ctx, client: calDavClient, keychain: keychain)
        taskRepository  = TaskRepository(context: ctx, client: calDavClient, statsRepo: statsRepository)
        noteRepository  = NoteRepository(context: ctx, client: calDavClient)
        issueRepository = IssueRepository(context: ctx, client: giteaClient, statsRepo: statsRepository)
        memoRepository  = MemoRepository(context: ctx, client: memosClient)
    }

    // ─── Sync all ─────────────────────────────────────────────────────────────

    /// Called by SyncScheduler and on app launch.
    func syncAll() async {
        let calendarHrefs = selectedCalendarHrefs()
        let repositories  = giteaRepositories()

        await withTaskGroup(of: Void.self) { group in
            group.addTask { await self.eventRepository.sync(calendarHrefs: calendarHrefs) }
            group.addTask { await self.taskRepository.sync(calendarHrefs: calendarHrefs) }
            group.addTask { await self.noteRepository.sync(calendarHrefs: calendarHrefs) }
            group.addTask { await self.issueRepository.sync(repositories: repositories) }
            group.addTask { await self.memoRepository.syncMemos() }
        }
        preferences.lastSyncDate = Date()
        writeWidgetSnapshot()
        WidgetCenter.shared.reloadAllTimelines()
        await DesktopBackgroundManager.shared.refreshIfEnabled()
    }

    private func writeWidgetSnapshot() {
        let now = Date()
        let weekFromNow = Calendar.current.date(byAdding: .day, value: 7, to: now) ?? now

        let events = eventRepository.events
            .filter { $0.start >= now && $0.start <= weekFromNow }
            .sorted { $0.start < $1.start }
            .prefix(5)
            .map { e in
                WidgetUpcomingEvent(
                    id: e.uid,
                    summary: e.summary,
                    startEpoch: e.start.timeIntervalSince1970,
                    calendarColor: nil
                )
            }

        let tasks = taskRepository.allTasks
            .filter {
                $0.status != .completed &&
                $0.status != .cancelled &&
                $0.due != nil &&
                $0.due! <= weekFromNow
            }
            .sorted { ($0.due ?? .distantFuture) < ($1.due ?? .distantFuture) }
            .prefix(5)
            .map { t in
                WidgetDueTask(
                    id: t.uid,
                    summary: t.summary,
                    dueEpoch: t.due?.timeIntervalSince1970,
                    isOverdue: (t.due ?? .distantFuture) < now,
                    priority: t.priority
                )
            }

        let snapshot = WidgetSnapshot(
            upcomingEvents: Array(events),
            dueSoonTasks: Array(tasks),
            openIssuesCount: issueRepository.openIssues.count,
            syncIntervalMinutes: preferences.syncIntervalMinutes
        )
        WidgetDataStore.save(snapshot)
    }

    // ─── Credential helpers ───────────────────────────────────────────────────

    private func selectedCalendarHrefs() -> [String] {
        guard let raw = keychain.get(CredentialKey.caldavSelectedCalendars) else { return [] }
        return (try? JSONDecoder().decode([CalDavCalendar].self, from: Data(raw.utf8)))?.map(\.href) ?? []
    }

    private func giteaRepositories() -> [String] {
        guard let raw = keychain.get(CredentialKey.giteaRepos) else { return [] }
        return (try? JSONDecoder().decode([String].self, from: Data(raw.utf8))) ?? []
    }
}

// ─── SwiftUI Environment key ─────────────────────────────────────────────────

import SwiftUI

private struct AppContainerKey: EnvironmentKey {
    static let defaultValue: AppContainer = .shared
}

extension EnvironmentValues {
    var appContainer: AppContainer {
        get { self[AppContainerKey.self] }
        set { self[AppContainerKey.self] = newValue }
    }
}
