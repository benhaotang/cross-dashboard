import SwiftUI
import CrossDashboardKit

/// Nested task list with quick-input bar.
/// Uses OutlineGroup to render parent–child subtask trees.
/// Mirrors TasksScreen on Android.
struct TasksView: View {

    @Environment(AppViewModel.self) private var appViewModel
    @Environment(\.appContainer) private var container
    @State private var viewModel = TasksViewModel()

    var body: some View {
        @Bindable var appVM = appViewModel
        VStack(spacing: 0) {
            taskList
            QuickInputBar(
                text: Binding(
                    get: { viewModel.quickInputText },
                    set: { viewModel.onQuickInputChanged($0) }
                ),
                parsed: viewModel.parsedTask,
                focusTrigger: viewModel.shouldFocusQuickInput,
                isSubmitting: viewModel.isSubmittingQuickInput,
                onSubmit: { viewModel.submitQuickInput() }
            )
        }
        .navigationTitle("Tasks")
        .toolbar { toolbarContent }
        .alert("Error", isPresented: Binding(
            get: { viewModel.errorMessage != nil },
            set: { if !$0 { viewModel.errorMessage = nil } }
        )) {
            Button("OK") { viewModel.errorMessage = nil }
        } message: {
            Text(viewModel.errorMessage ?? "")
        }
        .onChange(of: appViewModel.newTaskRequested) { _, requested in
            guard requested else { return }
            viewModel.shouldFocusQuickInput = true
            appViewModel.consumeNewTaskTrigger()
            Task { @MainActor in
                await Task.yield()
                viewModel.shouldFocusQuickInput = false
            }
        }
        .onChange(of: viewModel.selectedTaskID) { _, id in
            appViewModel.selectedTaskID = id
        }
        .onAppear {
            viewModel.selectedTaskID = appViewModel.selectedTaskID
        }
        .onChange(of: appViewModel.selectedTaskID) { _, id in
            viewModel.selectedTaskID = id
        }
    }

    // ─── Task list ────────────────────────────────────────────────────────────

    @ViewBuilder
    private var taskList: some View {
        if viewModel.isLoading && viewModel.allTasks.isEmpty {
            ProgressView("Loading tasks…")
                .frame(maxWidth: .infinity, maxHeight: .infinity)
        } else if viewModel.filteredRootTasks.isEmpty {
            ContentUnavailableView(
                emptyTitle,
                systemImage: "checklist",
                description: Text(emptyDescription)
            )
        } else {
            List(selection: Binding(
                get: { viewModel.selectedTaskID },
                set: { viewModel.selectedTaskID = $0 }
            )) {
                OutlineGroup(
                    taskTree,
                    id: \.id,
                    children: \.children
                ) { node in
                    TaskRow(task: node.task, viewModel: viewModel)
                        .tag(node.task.uid)
                }
            }
            .listStyle(.sidebar)
        }
    }

    // ─── Task tree ────────────────────────────────────────────────────────────

    private var taskTree: [TaskNode] {
        buildNodes(viewModel.filteredRootTasks)
    }

    private func buildNodes(_ tasks: [CalDavTask]) -> [TaskNode] {
        tasks.map { task in
            let subs = viewModel.subtasks(of: task.uid)
            return TaskNode(task: task, children: subs.isEmpty ? nil : buildNodes(subs))
        }
    }

    // ─── Toolbar ──────────────────────────────────────────────────────────────

    @ToolbarContentBuilder
    private var toolbarContent: some ToolbarContent {
        ToolbarItem(placement: .primaryAction) {
            if viewModel.isLoading {
                ProgressView().controlSize(.small)
            }
        }

        ToolbarItem(placement: .principal) {
            Picker("Filter", selection: Binding(
                get: { viewModel.filter },
                set: { viewModel.filter = $0 }
            )) {
                ForEach(TasksViewModel.Filter.allCases) { f in
                    Text(f.rawValue).tag(f)
                }
            }
            .pickerStyle(.segmented)
            .frame(maxWidth: 280)
            .accessibilityLabel("Task filter")
        }
    }

    // ─── Empty state strings ──────────────────────────────────────────────────

    private var emptyTitle: String {
        switch viewModel.filter {
        case .all:       return "No tasks yet"
        case .active:    return "All caught up!"
        case .completed: return "No completed tasks"
        }
    }

    private var emptyDescription: String {
        switch viewModel.filter {
        case .all, .active: return "Use the bar below to add your first task."
        case .completed:    return "Complete a task to see it here."
        }
    }
}

// ─── TaskRow ──────────────────────────────────────────────────────────────────

private struct TaskRow: View {

    let task: CalDavTask
    let viewModel: TasksViewModel

    var body: some View {
        HStack(spacing: 8) {
            // Completion toggle
            Button {
                viewModel.toggleComplete(task)
            } label: {
                Image(systemName: task.status == .completed ? "checkmark.circle.fill" : "circle")
                    .foregroundStyle(task.status == .completed ? .green : .secondary)
            }
            .buttonStyle(.plain)
            .accessibilityLabel(task.status == .completed ? "Mark incomplete" : "Mark complete")

            CalendarColorDot(calendarHref: task.calendarHref)

            VStack(alignment: .leading, spacing: 2) {
                Text(task.summary)
                    .strikethrough(task.status == .completed)
                    .lineLimit(1)
                    .font(.body)

                if !task.categories.isEmpty || task.due != nil {
                    HStack(spacing: 6) {
                        if let due = task.due {
                            Label(due.formatted(date: .abbreviated, time: .omitted), systemImage: "calendar")
                                .font(.caption)
                                .foregroundStyle(due < Date() && task.status != .completed ? .red : .secondary)
                        }
                        ForEach(task.categories.prefix(3), id: \.self) { tag in
                            TagChip(tag: tag)
                        }
                    }
                }
            }

            Spacer()
            PriorityChip(priority: task.priority)
        }
        .padding(.vertical, 2)
        .contextMenu {
            Button("Toggle Complete") { viewModel.toggleComplete(task) }
            Divider()
            Button {
                NSPasteboard.general.clearContents()
                NSPasteboard.general.setString(task.uid, forType: .string)
            } label: {
                Label("Copy UID", systemImage: "doc.on.clipboard")
            }
            Divider()
            Button("Delete", role: .destructive) { viewModel.deleteTask(task) }
        }
        .keyboardShortcut(.delete, modifiers: .command)
        .accessibilityElement(children: .combine)
        .accessibilityLabel(accessibilityLabel)
    }

    private var accessibilityLabel: String {
        var parts = [task.summary]
        if let due = task.due { parts.append("due \(due.formatted(date: .abbreviated, time: .omitted))") }
        if task.status == .completed { parts.append("completed") }
        return parts.joined(separator: ", ")
    }
}

// ─── TaskNode: wraps CalDavTask with pre-computed children for OutlineGroup ───

private struct TaskNode: Identifiable {
    let task: CalDavTask
    let children: [TaskNode]?
    var id: String { task.uid }
}
