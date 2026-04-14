import Foundation
import SwiftData
import CrossDashboardKit

/// AppContainer creates and vends all singletons for the app.
/// It is the macOS equivalent of Hilt's singleton scope in the Android app.
/// Inject via `@Environment(\.appContainer)` from SwiftUI views.
@Observable
@MainActor
final class AppContainer {

    // ─── Shared instance ──────────────────────────────────────────────────────

    nonisolated(unsafe) static let shared: AppContainer = MainActor.assumeIsolated { AppContainer() }

    // ─── Data layer ───────────────────────────────────────────────────────────

    let keychain: KeychainStore
    let preferences: AppPreferences
    let persistence: PersistenceController

    let calDavClient: CalDavClient
    let giteaClient: GiteaClient
    let memosClient: MemosClient
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
