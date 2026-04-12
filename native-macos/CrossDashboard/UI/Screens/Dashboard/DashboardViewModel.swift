import Foundation
import Observation
import CrossDashboardKit

@Observable
@MainActor
final class DashboardViewModel {

    // ─── Published state ──────────────────────────────────────────────────────

    var stats: [DailyStats] = []
    var upcomingEvents: [CalendarEvent] = []
    var dueSoonTasks: [CalDavTask] = []
    var openIssueCount: Int = 0
    var isLoading: Bool = false
    var errorMessage: String?

    // ─── Dependencies ─────────────────────────────────────────────────────────

    private let container: AppContainer

    init(container: AppContainer = .shared) {
        self.container = container
    }

    // ─── Load ─────────────────────────────────────────────────────────────────

    func load() async {
        isLoading = true
        errorMessage = nil
        defer { isLoading = false }

        stats          = container.statsRepository.getRange(startDaysAgo: 7)
        upcomingEvents = container.eventRepository.getUpcoming(limit: 5)
        dueSoonTasks   = container.taskRepository.getDueSoon(before: Date().addingTimeInterval(7 * 86400))
        openIssueCount = container.issueRepository.allIssues.filter { $0.state == "open" }.count
    }

    // ─── Derived helpers ──────────────────────────────────────────────────────

    /// 7-day chart data: (date label, combined count)
    var chartData: [(label: String, tasks: Int, pomodoros: Int, issues: Int)] {
        let formatter = DateFormatter()
        formatter.dateFormat = "EEE"
        let calendar = Calendar.current

        return (0..<7).reversed().map { offset in
            let date = calendar.date(byAdding: .day, value: -offset, to: Date()) ?? Date()
            let key  = DailyStats.dateKey(for: date)
            let stat = stats.first { $0.date == key }
            return (
                label:    formatter.string(from: date),
                tasks:    stat?.tasksCompleted ?? 0,
                pomodoros: stat?.pomodoroSessions ?? 0,
                issues:   stat?.issuesClosed ?? 0
            )
        }
    }

    var totalTasksThisWeek: Int { stats.reduce(0) { $0 + $1.tasksCompleted } }
    var totalPomodorosThisWeek: Int { stats.reduce(0) { $0 + $1.pomodoroSessions } }
}

// ─── DailyStats helpers ───────────────────────────────────────────────────────

private extension DailyStats {
    static func dateKey(for date: Date) -> String {
        let fmt = DateFormatter()
        fmt.dateFormat = "yyyy-MM-dd"
        return fmt.string(from: date)
    }
}
