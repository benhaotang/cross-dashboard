import Foundation
import Observation
import CrossDashboardKit

@Observable
@MainActor
final class EventsViewModel {

    // ─── Filter ───────────────────────────────────────────────────────────────

    enum Filter: String, CaseIterable, Identifiable {
        case day   = "Day"
        case week  = "Week"
        case month = "Month"
        var id: String { rawValue }
    }

    var filter: Filter = .week
    var searchText: String = ""
    var selectedEventID: String?
    var isLoading: Bool = false
    var errorMessage: String?

    // ─── Dependencies ─────────────────────────────────────────────────────────

    private let container: AppContainer

    init(container: AppContainer = .shared) {
        self.container = container
    }

    // ─── Derived data ─────────────────────────────────────────────────────────

    var allEvents: [CalendarEvent] { container.eventRepository.events }

    var filteredEvents: [CalendarEvent] {
        let now = Date()
        let cal = Calendar.current
        let cutoff: Date = {
            switch filter {
            case .day:   return cal.date(byAdding: .day, value: 1, to: cal.startOfDay(for: now)) ?? now
            case .week:  return cal.date(byAdding: .weekOfYear, value: 1, to: now) ?? now
            case .month: return cal.date(byAdding: .month, value: 1, to: now) ?? now
            }
        }()

        let base = allEvents
            .filter { $0.start >= cal.startOfDay(for: now) && $0.start <= cutoff }
            .sorted { $0.start < $1.start }

        guard !searchText.isEmpty else { return base }
        let q = searchText.lowercased()
        return base.filter {
            $0.summary.lowercased().contains(q) ||
            ($0.description?.lowercased().contains(q) ?? false) ||
            ($0.location?.lowercased().contains(q) ?? false)
        }
    }

    var selectedEvent: CalendarEvent? {
        guard let id = selectedEventID else { return nil }
        return allEvents.first { $0.uid == id }
    }

    // ─── Actions ──────────────────────────────────────────────────────────────

    func sync() async {
        isLoading = true
        defer { isLoading = false }
        await container.syncAll()
    }

    func delete(_ event: CalendarEvent) {
        Task {
            do {
                try await container.eventRepository.delete(event)
            } catch {
                errorMessage = error.localizedDescription
            }
        }
    }
}
