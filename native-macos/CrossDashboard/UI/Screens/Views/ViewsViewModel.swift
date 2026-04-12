import Foundation
import Observation
import CrossDashboardKit

@Observable
@MainActor
final class ViewsViewModel {

    enum ViewMode: String, CaseIterable, Identifiable {
        case kanban = "Kanban"
        case covey  = "Covey"
        var id: String { rawValue }
    }

    var mode: ViewMode = .kanban
    var kanbanColumns: [String] { container.preferences.kanbanColumns }

    // Dragging state
    var draggingTaskID: String?

    // Assign modal
    var assigningTask: CalDavTask?
    var assignColumnInput: String = ""

    // ─── Dependencies ─────────────────────────────────────────────────────────

    private let container: AppContainer

    init(container: AppContainer = .shared) {
        self.container = container
    }

    // ─── Kanban data ──────────────────────────────────────────────────────────

    var allTasks: [CalDavTask] { container.taskRepository.allTasks }

    func tasks(inColumn column: String) -> [CalDavTask] {
        allTasks.filter { task in
            task.categories.contains(where: { $0.lowercased() == column.lowercased() })
        }
    }

    // ─── Covey quadrant data ──────────────────────────────────────────────────
    // Urgency = has due date within 2 days; Importance = priority <= 4 (high)

    var importantUrgent: [CalDavTask] {
        allTasks.filter { isImportant($0) && isUrgent($0) }
    }

    var importantNotUrgent: [CalDavTask] {
        allTasks.filter { isImportant($0) && !isUrgent($0) }
    }

    var notImportantUrgent: [CalDavTask] {
        allTasks.filter { !isImportant($0) && isUrgent($0) }
    }

    var notImportantNotUrgent: [CalDavTask] {
        allTasks.filter { !isImportant($0) && !isUrgent($0) }
    }

    // ─── Actions ──────────────────────────────────────────────────────────────

    func moveTask(_ task: CalDavTask, toColumn column: String) {
        var newCategories = task.categories.filter { cat in
            !kanbanColumns.contains(where: { $0.lowercased() == cat.lowercased() })
        }
        newCategories.append(column)

        let updated = CalDavTask(
            uid: task.uid,
            summary: task.summary,
            description: task.description,
            status: task.status,
            priority: task.priority,
            percentComplete: task.percentComplete,
            due: task.due,
            dtstart: task.dtstart,
            completed: task.completed,
            created: task.created,
            lastModified: Date(),
            categories: newCategories,
            location: task.location,
            parentUid: task.parentUid,
            calendarHref: task.calendarHref,
            etag: task.etag,
            href: task.href
        )

        Task {
            _ = try? await container.taskRepository.update(updated)
        }
    }

    func showAssignModal(for task: CalDavTask) {
        assigningTask = task
        assignColumnInput = task.categories.first(where: { cat in
            kanbanColumns.contains(where: { $0.lowercased() == cat.lowercased() })
        }) ?? ""
    }

    func confirmAssign() {
        guard let task = assigningTask, !assignColumnInput.isEmpty else {
            assigningTask = nil
            return
        }
        moveTask(task, toColumn: assignColumnInput)
        assigningTask = nil
        assignColumnInput = ""
    }

    // ─── Helpers ──────────────────────────────────────────────────────────────

    private func isImportant(_ task: CalDavTask) -> Bool {
        task.priority > 0 && task.priority <= 4
    }

    private func isUrgent(_ task: CalDavTask) -> Bool {
        guard let due = task.due else { return false }
        return due <= Calendar.current.date(byAdding: .day, value: 2, to: Date()) ?? Date()
    }
}
