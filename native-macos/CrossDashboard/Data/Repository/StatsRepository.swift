import Foundation
import SwiftData
import CrossDashboardKit

/// Mirrors StatsRepository.kt.
@MainActor
final class StatsRepository {

    private let context: ModelContext

    init(context: ModelContext) {
        self.context = context
    }

    func getRange(startDaysAgo: Int) -> [DailyStats] {
        let calendar = Calendar.current
        let start = calendar.date(byAdding: .day, value: -startDaysAgo, to: calendar.startOfDay(for: Date()))!
        let startStr = isoDate(start)

        let descriptor = FetchDescriptor<StatsModel>(
            predicate: #Predicate { $0.date >= startStr },
            sortBy: [SortDescriptor(\.date)]
        )
        let models = (try? context.fetch(descriptor)) ?? []
        return models.map { $0.toDomain() }
    }

    func incrementTasksCompleted() {
        increment(.tasks)
    }

    func incrementPomodoro() {
        increment(.pomodoro)
    }

    func incrementIssuesClosed() {
        increment(.issues)
    }

    private enum StatField { case tasks, pomodoro, issues }

    private func increment(_ field: StatField) {
        let today = isoDate(Date())
        let descriptor = FetchDescriptor<StatsModel>(
            predicate: #Predicate { $0.date == today }
        )
        if let existing = (try? context.fetch(descriptor))?.first {
            switch field {
            case .tasks:    existing.tasksCompleted    += 1
            case .pomodoro: existing.pomodoroSessions  += 1
            case .issues:   existing.issuesClosed      += 1
            }
        } else {
            let model = StatsModel(date: today)
            switch field {
            case .tasks:    model.tasksCompleted    = 1
            case .pomodoro: model.pomodoroSessions  = 1
            case .issues:   model.issuesClosed      = 1
            }
            context.insert(model)
        }
        try? context.save()
    }

    private func isoDate(_ date: Date) -> String {
        let f = DateFormatter()
        f.dateFormat = "yyyy-MM-dd"
        f.locale = Locale(identifier: "en_US_POSIX")
        return f.string(from: date)
    }
}
