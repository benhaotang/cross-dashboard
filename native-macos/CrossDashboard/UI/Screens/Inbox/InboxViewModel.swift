import Foundation
import Observation
import CrossDashboardKit

@Observable
@MainActor
final class InboxViewModel {

    enum ItemType: String, CaseIterable, Identifiable {
        case all       = "All"
        case events    = "Events"
        case tasks     = "Tasks"
        case issues    = "Issues"
        var id: String { rawValue }
    }

    var filter: ItemType = .all
    var searchText: String = ""
    var isLoading: Bool = false

    // ─── Dependencies ─────────────────────────────────────────────────────────

    private let container: AppContainer

    init(container: AppContainer = .shared) {
        self.container = container
    }

    // ─── Derived data ─────────────────────────────────────────────────────────

    var allItems: [InboxItem] {
        var items: [InboxItem] = []

        let now = Date()
        let horizon = Calendar.current.date(byAdding: .day, value: 7, to: now) ?? now

        // Upcoming events (next 7 days)
        let events = container.eventRepository.events
            .filter { $0.start >= now && $0.start <= horizon }
            .sorted { $0.start < $1.start }
        for event in events {
            let minutes = Int(event.end.timeIntervalSince(event.start) / 60)
            items.append(.event(event, durationMinutes: minutes))
        }

        // Active tasks with due date in next 7 days
        let tasks = container.taskRepository.allTasks
            .filter {
                $0.status != .completed && $0.status != .cancelled &&
                ($0.due.map { $0 <= horizon } ?? false)
            }
            .sorted { ($0.due ?? .distantFuture) < ($1.due ?? .distantFuture) }
        for task in tasks {
            let estimated = extractEstimatedMinutes(from: task)
            items.append(.task(task, estimatedMinutes: estimated))
        }

        // Open issues
        let issues = container.issueRepository.openIssues
        for issue in issues.prefix(20) {
            let estimated = extractEstimatedMinutes(fromBody: issue.body)
            items.append(.issue(issue, estimatedMinutes: estimated))
        }

        return items
    }

    var filteredItems: [InboxItem] {
        let base: [InboxItem]
        switch filter {
        case .all:    base = allItems
        case .events: base = allItems.filter { if case .event = $0 { return true }; return false }
        case .tasks:  base = allItems.filter { if case .task = $0 { return true }; return false }
        case .issues: base = allItems.filter { if case .issue = $0 { return true }; return false }
        }
        guard !searchText.isEmpty else { return base }
        let q = searchText.lowercased()
        return base.filter { item in
            switch item {
            case .event(let e, _):  return e.summary.lowercased().contains(q)
            case .task(let t, _):   return t.summary.lowercased().contains(q)
            case .issue(let i, _):  return i.title.lowercased().contains(q)
            case .milestone(let m): return m.title.lowercased().contains(q)
            }
        }
    }

    /// Total estimated minutes across all filtered items that have a value.
    var totalEstimatedMinutes: Int {
        filteredItems.reduce(0) { sum, item in
            switch item {
            case .event(_, let d): return sum + d
            case .task(_, let m):  return sum + (m ?? 0)
            case .issue(_, let m): return sum + (m ?? 0)
            case .milestone:       return sum
            }
        }
    }

    var totalEstimatedLabel: String {
        let total = totalEstimatedMinutes
        guard total > 0 else { return "" }
        let h = total / 60; let m = total % 60
        if h == 0 { return "\(m) min" }
        return m == 0 ? "\(h) hr" : "\(h) hr \(m) min"
    }

    // ─── Actions ──────────────────────────────────────────────────────────────

    func sync() async {
        isLoading = true
        defer { isLoading = false }
        await container.syncAll()
    }

    // ─── Helpers ──────────────────────────────────────────────────────────────

    /// Extracts #Xm / #Xh tags from task categories or description.
    private func extractEstimatedMinutes(from task: CalDavTask) -> Int? {
        let sources = task.categories + [task.description ?? ""]
        return extractEstimatedMinutes(fromTokens: sources.joined(separator: " "))
    }

    private func extractEstimatedMinutes(fromBody body: String) -> Int? {
        extractEstimatedMinutes(fromTokens: body)
    }

    private func extractEstimatedMinutes(fromTokens text: String) -> Int? {
        var total = 0
        // Match #Xh or #Xm
        let pattern = #/#(\d+)(h|m)/#
        for match in text.matches(of: pattern) {
            let value = Int(match.1) ?? 0
            let unit  = match.2
            total += unit == "h" ? value * 60 : value
        }
        return total > 0 ? total : nil
    }
}
