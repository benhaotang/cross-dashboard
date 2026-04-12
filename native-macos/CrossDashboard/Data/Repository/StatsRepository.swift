import Foundation
import SwiftData
import CrossDashboardKit

/// Mirrors StatsRepository.kt.
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

    func incrementTasksCompleted() async {
        await MainActor.run { increment(field: \.tasksCompleted) }
    }

    func incrementPomodoro() async {
        await MainActor.run { increment(field: \.pomodoroSessions) }
    }

    func incrementIssuesClosed() async {
        await MainActor.run { increment(field: \.issuesClosed) }
    }

    private func increment(field: WritableKeyPath<StatsModel, Int>) {
        let today = isoDate(Date())
        let descriptor = FetchDescriptor<StatsModel>(
            predicate: #Predicate { $0.date == today }
        )
        if let existing = (try? context.fetch(descriptor))?.first {
            existing[keyPath: field] += 1
        } else {
            let model = StatsModel(date: today)
            model[keyPath: field] = 1
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
