import Foundation
import SwiftData
import CrossDashboardKit

/// Room is the source of truth; UI observes via ModelContext fetch.
/// Mirrors EventRepository.kt.
@Observable
@MainActor
final class EventRepository {

    private let context: ModelContext
    private let client: CalDavClient
    private let keychain: KeychainStore

    var events: [CalendarEvent] = []

    init(context: ModelContext, client: CalDavClient, keychain: KeychainStore = .shared) {
        self.context = context
        self.client  = client
        self.keychain = keychain
        loadFromDB()
    }

    func loadFromDB() {
        var descriptor = FetchDescriptor<EventModel>()
        descriptor.includePendingChanges = false
        let models = (try? context.fetch(descriptor)) ?? []
        events = models.map { $0.toDomain() }
    }

    @discardableResult
    func sync(calendarHrefs: [String]) async -> Bool {
        guard !calendarHrefs.isEmpty else { return true }
        let thirtyDaysAgo = Date().addingTimeInterval(-30 * 86400)
        let sixMonthsOut  = Date().addingTimeInterval(180 * 86400)
        guard let fresh = await client.fetchEvents(
            calendarHrefs: calendarHrefs,
            from: thirtyDaysAgo,
            to: sixMonthsOut
        ) else { return false }
        do {
            try context.transaction {
                try context.delete(model: EventModel.self)
                fresh.forEach { context.insert(EventModel(from: $0)) }
            }
            loadFromDB()
            return true
        } catch {
            context.rollback()
            return false
        }
    }

    func getUpcoming(limit: Int = 5) -> [CalendarEvent] {
        let now = Date()
        return events
            .filter { $0.start >= now }
            .sorted { $0.start < $1.start }
            .prefix(limit)
            .map { $0 }
    }

    func create(_ event: CalendarEvent, calendarHref: String) async throws -> CalendarEvent {
        let saved = try await client.createEvent(event, calendarHref: calendarHref)
        await MainActor.run {
            context.insert(EventModel(from: saved))
            try? context.save()
            loadFromDB()
        }
        return saved
    }

    func delete(_ event: CalendarEvent) async throws {
        try await client.deleteEvent(event)
        await MainActor.run {
            let models = (try? context.fetch(FetchDescriptor<EventModel>())) ?? []
            models.filter { $0.uid == event.uid }.forEach { context.delete($0) }
            try? context.save()
            loadFromDB()
        }
    }

    private func selectedCalendarHrefs() -> [String] {
        guard let raw = keychain.get(CredentialKey.caldavSelectedCalendars) else { return [] }
        return (try? JSONDecoder().decode([CalDavCalendar].self, from: Data(raw.utf8)))?.map(\.href) ?? []
    }
}
