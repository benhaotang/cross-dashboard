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

    enum ItemType: String, CaseIterable, Identifiable {
        case all = "All", tasks = "Tasks", issues = "Issues"
        var id: String { rawValue }
    }

    enum DateFilter: String, CaseIterable, Identifiable {
        case all = "All", today = "Today", tomorrow = "Tomorrow", thisWeek = "This week"
        var id: String { rawValue }
    }

    var mode: ViewMode = .kanban
    var itemType: ItemType = .all
    var dateFilter: DateFilter = .all

    var kanbanColumns: [String] { container.preferences.kanbanColumns }

    // Assign modal (single task)
    var assigningTask: CalDavTask?
    var assignColumnInput: String = ""

    // Bulk assign modal — target column + which tasks to show
    var bulkAssignTarget: String? = nil

    // Column config modal
    var editingColumns: [String]? = nil

    // ─── Dependencies ─────────────────────────────────────────────────────────

    private let container: AppContainer

    init(container: AppContainer = .shared) {
        self.container = container
    }

    // ─── Kanban data ──────────────────────────────────────────────────────────

    /// All open (non-completed, non-cancelled) tasks — mirrors Android's filter.
    var allTasks: [CalDavTask] {
        guard itemType != .issues else { return [] }
        container.taskRepository.allTasks.filter {
            $0.status != .completed && $0.status != .cancelled && matchesDate($0.due)
        }
    }

    var allIssues: [GiteaIssue] {
        guard itemType != .tasks else { return [] }
        return container.issueRepository.openIssues.filter { matchesDate($0.milestoneDueOn) }
    }

    func clearFilters() {
        itemType = .all
        dateFilter = .all
    }

    private func matchesDate(_ date: Date?) -> Bool {
        switch dateFilter {
        case .all: return true
        case .today: return date.map { Calendar.current.isDateInToday($0) } == true
        case .tomorrow: return date.map { Calendar.current.isDateInTomorrow($0) } == true
        case .thisWeek:
            guard let date else { return false }
            return Calendar.current.dateInterval(of: .weekOfYear, for: Date())?.contains(date) == true
        }
    }

    func issues(inColumn column: String) -> [GiteaIssue] {
        allIssues.filter { issue in
            issue.labels.contains { $0.lowercased() == column.lowercased() }
        }
    }

    var unassignedIssues: [GiteaIssue] {
        allIssues.filter { issue in
            issue.labels.none(where: { label in
                kanbanColumns.contains { $0.lowercased() == label.lowercased() }
            })
        }
    }

    func issues(withLabel label: String) -> [GiteaIssue] {
        allIssues.filter { issue in
            issue.labels.contains { $0.lowercased() == label.lowercased() }
        }
    }

    func tasks(inColumn column: String) -> [CalDavTask] {
        allTasks.filter { task in
            task.categories.contains(where: { $0.lowercased() == column.lowercased() })
        }
    }

    /// Tasks not assigned to any configured kanban column.
    var unassignedTasks: [CalDavTask] {
        allTasks.filter { task in
            task.categories.none(where: { cat in
                kanbanColumns.contains(where: { $0.lowercased() == cat.lowercased() })
            })
        }
    }

    /// Tasks not yet in the given column — used by the bulk assign sheet.
    func tasksNotIn(column: String) -> [CalDavTask] {
        allTasks.filter { task in
            !task.categories.contains(where: { $0.lowercased() == column.lowercased() })
        }
    }

    // ─── Covey quadrant data ──────────────────────────────────────────────────
    // Urgency = has due date within 2 days; Importance = priority 1–4 (high)

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

    // ─── Move task ────────────────────────────────────────────────────────────

    func moveTask(_ task: CalDavTask, toColumn column: String) {
        var newCategories = task.categories.filter { cat in
            !kanbanColumns.contains(where: { $0.lowercased() == cat.lowercased() })
        }
        newCategories.append(column)
        persistTask(task, newCategories: newCategories)
    }

    func removeFromColumn(_ task: CalDavTask, column: String) {
        let newCategories = task.categories.filter { $0.lowercased() != column.lowercased() }
        persistTask(task, newCategories: newCategories)
    }

    private func persistTask(_ task: CalDavTask, newCategories: [String]) {
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
        Task { _ = try? await container.taskRepository.update(updated) }
    }

    // ─── Assign modal ─────────────────────────────────────────────────────────

    func showAssignModal(for task: CalDavTask) {
        assigningTask = task
        assignColumnInput = task.categories.first(where: { cat in
            kanbanColumns.contains(where: { $0.lowercased() == cat.lowercased() })
        }) ?? kanbanColumns.first ?? ""
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

    // ─── Bulk assign ──────────────────────────────────────────────────────────

    func openBulkAssign(targetColumn: String? = nil) {
        bulkAssignTarget = targetColumn ?? kanbanColumns.first ?? ""
    }

    func closeBulkAssign() { bulkAssignTarget = nil }

    func setBulkAssignTarget(_ column: String) { bulkAssignTarget = column }

    /// Assign task to column without closing the sheet (batch-assign flow).
    func assignFromBulk(_ task: CalDavTask, toColumn column: String) {
        moveTask(task, toColumn: column)
    }

    // ─── Column config ────────────────────────────────────────────────────────

    func openColumnConfig() { editingColumns = kanbanColumns }

    func closeColumnConfig() { editingColumns = nil }

    func saveColumns(_ columns: [String]) {
        let trimmed = columns
            .map { $0.trimmingCharacters(in: .whitespaces).lowercased() }
            .filter { !$0.isEmpty }
        guard !trimmed.isEmpty else { return }
        container.preferences.kanbanColumns = trimmed
        editingColumns = nil
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

private extension Array {
    func none(where predicate: (Element) -> Bool) -> Bool {
        !contains(where: predicate)
    }
}
