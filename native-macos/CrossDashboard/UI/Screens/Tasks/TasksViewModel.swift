import Foundation
import Observation
import CrossDashboardKit

@Observable
@MainActor
final class TasksViewModel {

    // ─── Filter ───────────────────────────────────────────────────────────────

    enum Filter: String, CaseIterable, Identifiable {
        case all       = "All"
        case active    = "Active"
        case completed = "Completed"
        var id: String { rawValue }
    }

    var filter: Filter = .active
    var selectedTags: Set<String> = []

    // ─── State ────────────────────────────────────────────────────────────────

    var selectedTaskID: String?
    var isLoading: Bool = false
    var errorMessage: String?

    // ─── Quick input ──────────────────────────────────────────────────────────

    var quickInputText: String = ""
    var parsedTask: ParsedTask?
    var isSubmittingQuickInput: Bool = false
    var shouldFocusQuickInput: Bool = false

    // ─── Dependencies ─────────────────────────────────────────────────────────

    private let container: AppContainer

    init(container: AppContainer = .shared) {
        self.container = container
    }

    // ─── Computed views into repository data ──────────────────────────────────

    var allTasks: [CalDavTask] { container.taskRepository.allTasks }

    var filteredRootTasks: [CalDavTask] {
        let roots = allTasks.filter { $0.parentUid == nil }
        return roots.filter(matchesStatus).filter { task in
            selectedTags.allSatisfy { task.categories.contains($0) }
        }
    }

    func subtasks(of parentUid: String) -> [CalDavTask] {
        container.taskRepository.subtasks(of: parentUid)
            .filter(matchesStatus)
            .filter { task in selectedTags.allSatisfy { task.categories.contains($0) } }
    }

    var allTags: [String] {
        Array(Set(allTasks.flatMap(\.categories))).sorted {
            $0.localizedCaseInsensitiveCompare($1) == .orderedAscending
        }
    }

    func clearFilters() {
        filter = .active
        selectedTags = []
    }

    private func matchesStatus(_ task: CalDavTask) -> Bool {
        switch filter {
        case .all: return true
        case .active: return task.status != .completed && task.status != .cancelled
        case .completed: return task.status == .completed
        }
    }

    var selectedTask: CalDavTask? {
        guard let id = selectedTaskID else { return nil }
        return allTasks.first { $0.uid == id }
    }

    // ─── Actions ──────────────────────────────────────────────────────────────

    func toggleComplete(_ task: CalDavTask) {
        Task {
            do {
                _ = try await container.taskRepository.toggleComplete(task)
            } catch {
                errorMessage = error.localizedDescription
            }
        }
    }

    func deleteTask(_ task: CalDavTask) {
        Task {
            do {
                try await container.taskRepository.delete(task)
            } catch {
                errorMessage = error.localizedDescription
            }
        }
    }

    // ─── Quick input ──────────────────────────────────────────────────────────

    func onQuickInputChanged(_ text: String) {
        quickInputText = text
        parsedTask = text.isEmpty ? nil : TaskInputParser.parse(input: text, defaults: container.preferences.taskDefaults)
    }

    func submitQuickInput() {
        guard !quickInputText.trimmingCharacters(in: .whitespaces).isEmpty else { return }
        let parsed = parsedTask ?? TaskInputParser.parse(input: quickInputText, defaults: container.preferences.taskDefaults)
        let calendarHref = container.keychain.get(CredentialKey.caldavDefaultTaskCalendar) ?? ""
        guard !calendarHref.isEmpty else {
            errorMessage = "No default task calendar configured. Go to Settings → CalDAV."
            return
        }

        isSubmittingQuickInput = true
        let text = quickInputText
        quickInputText = ""
        parsedTask = nil

        Task {
            defer { isSubmittingQuickInput = false }
            do {
                let task = CalDavTask(
                    summary: parsed.summary,
                    priority: parsed.priority,
                    due: parsed.due,
                    categories: parsed.categories,
                    calendarHref: calendarHref
                )
                _ = try await container.taskRepository.create(task, calendarHref: calendarHref)
            } catch {
                errorMessage = "Failed to create task: \(error.localizedDescription)"
                quickInputText = text
            }
        }
    }

    func sync() async {
        isLoading = true
        defer { isLoading = false }
        await container.syncAll()
    }
}
