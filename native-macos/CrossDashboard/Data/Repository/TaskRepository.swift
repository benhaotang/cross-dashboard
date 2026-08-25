import Foundation
import SwiftData
import CrossDashboardKit

/// Mirrors TaskRepository.kt.
@Observable
@MainActor
final class TaskRepository {

    private let context: ModelContext
    private let client: CalDavClient
    private let statsRepo: StatsRepository

    var allTasks: [CalDavTask] = []

    var activeTasks: [CalDavTask] {
        allTasks.filter { $0.status != .completed && $0.status != .cancelled }
    }

    var completedTasks: [CalDavTask] {
        allTasks.filter { $0.status == .completed }
    }

    init(context: ModelContext, client: CalDavClient, statsRepo: StatsRepository) {
        self.context   = context
        self.client    = client
        self.statsRepo = statsRepo
        loadFromDB()
    }

    func loadFromDB() {
        var descriptor = FetchDescriptor<TaskModel>()
        descriptor.includePendingChanges = false
        let models = (try? context.fetch(descriptor)) ?? []
        allTasks = models.map { $0.toDomain() }
    }

    func subtasks(of parentUid: String) -> [CalDavTask] {
        allTasks.filter { $0.parentUid == parentUid }
    }

    @discardableResult
    func sync(calendarHrefs: [String]) async -> Bool {
        guard !calendarHrefs.isEmpty else { return true }
        guard let fresh = await client.fetchTasks(calendarHrefs: calendarHrefs) else { return false }
        do {
            try context.transaction {
                try context.delete(model: TaskModel.self)
                fresh.forEach { context.insert(TaskModel(from: $0)) }
            }
            loadFromDB()
            return true
        } catch {
            context.rollback()
            return false
        }
    }

    func create(_ task: CalDavTask, calendarHref: String) async throws -> CalDavTask {
        let saved = try await client.createTask(task, calendarHref: calendarHref)
        await MainActor.run {
            context.insert(TaskModel(from: saved))
            try? context.save()
            loadFromDB()
        }
        return saved
    }

    func update(_ task: CalDavTask) async throws {
        try await client.updateTask(task)
        await MainActor.run {
            upsertInDB(task)
        }
    }

    func delete(_ task: CalDavTask) async throws {
        try await client.deleteTask(task)
        await MainActor.run {
            let models = (try? context.fetch(FetchDescriptor<TaskModel>())) ?? []
            models.filter { $0.uid == task.uid }.forEach { context.delete($0) }
            try? context.save()
            loadFromDB()
        }
    }

    func toggleComplete(_ task: CalDavTask) async throws -> CalDavTask {
        let wasCompleted = task.status == .completed
        let updated: CalDavTask
        if wasCompleted {
            updated = CalDavTask(
                uid: task.uid, summary: task.summary, description: task.description,
                status: .needsAction, priority: task.priority, percentComplete: 0,
                due: task.due, dtstart: task.dtstart, completed: nil,
                created: task.created, lastModified: Date(),
                categories: task.categories, location: task.location, parentUid: task.parentUid,
                calendarHref: task.calendarHref, etag: task.etag, href: task.href
            )
        } else {
            updated = CalDavTask(
                uid: task.uid, summary: task.summary, description: task.description,
                status: .completed, priority: task.priority, percentComplete: 100,
                due: task.due, dtstart: task.dtstart, completed: Date(),
                created: task.created, lastModified: Date(),
                categories: task.categories, location: task.location, parentUid: task.parentUid,
                calendarHref: task.calendarHref, etag: task.etag, href: task.href
            )
        }
        try await update(updated)
        if !wasCompleted {
            statsRepo.incrementTasksCompleted()
        }
        return updated
    }

    func getDueSoon(before: Date, limit: Int = 5) -> [CalDavTask] {
        activeTasks
            .filter { task in task.due.map { $0 <= before } ?? false }
            .sorted { ($0.due ?? .distantFuture) < ($1.due ?? .distantFuture) }
            .prefix(limit)
            .map { $0 }
    }

    private func upsertInDB(_ task: CalDavTask) {
        let models = (try? context.fetch(FetchDescriptor<TaskModel>())) ?? []
        if let existing = models.first(where: { $0.uid == task.uid }) {
            context.delete(existing)
        }
        context.insert(TaskModel(from: task))
        try? context.save()
        loadFromDB()
    }
}
