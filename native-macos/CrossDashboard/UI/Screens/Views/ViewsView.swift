import SwiftUI
import CrossDashboardKit

/// Kanban board + Covey Four Quadrants view.
/// Mirrors ViewsScreen on Android.
struct ViewsView: View {

    @State private var viewModel = ViewsViewModel()

    var body: some View {
        @Bindable var vm = viewModel
        Group {
            switch viewModel.mode {
            case .kanban:
                KanbanBoard(viewModel: viewModel)
            case .covey:
                CoveyQuadrants(viewModel: viewModel)
            }
        }
        .navigationTitle("Views")
        .toolbar {
            ToolbarItem(placement: .primaryAction) {
                Picker("View mode", selection: $vm.mode) {
                    ForEach(ViewsViewModel.ViewMode.allCases) { m in
                        Label(m.rawValue, systemImage: m == .kanban ? "kanban" : "square.grid.2x2")
                            .tag(m)
                    }
                }
                .pickerStyle(.segmented)
                .accessibilityLabel("Switch between Kanban and Covey views")
            }
        }
        .sheet(item: $vm.assigningTask) { task in
            AssignColumnModal(
                task: task,
                columns: viewModel.kanbanColumns,
                selectedColumn: $vm.assignColumnInput,
                onConfirm: { viewModel.confirmAssign() },
                onCancel: { viewModel.assigningTask = nil }
            )
        }
    }
}

// ─── KanbanBoard ──────────────────────────────────────────────────────────────

private struct KanbanBoard: View {
    let viewModel: ViewsViewModel

    var body: some View {
        ScrollView(.horizontal, showsIndicators: false) {
            HStack(alignment: .top, spacing: 16) {
                ForEach(viewModel.kanbanColumns, id: \.self) { column in
                    KanbanColumn(
                        title: column,
                        tasks: viewModel.tasks(inColumn: column),
                        onMove: { task, target in
                            viewModel.moveTask(task, toColumn: target)
                        },
                        onAssign: { task in
                            viewModel.showAssignModal(for: task)
                        },
                        allColumns: viewModel.kanbanColumns
                    )
                }
            }
            .padding()
        }
    }
}

private struct KanbanColumn: View {
    let title: String
    let tasks: [CalDavTask]
    let onMove: (CalDavTask, String) -> Void
    let onAssign: (CalDavTask) -> Void
    let allColumns: [String]

    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            HStack {
                Text(title.capitalized)
                    .font(.headline)
                Spacer()
                Text("\(tasks.count)")
                    .font(.caption)
                    .foregroundStyle(.secondary)
                    .padding(.horizontal, 6)
                    .padding(.vertical, 2)
                    .background(Capsule().fill(Color.accentColor.opacity(0.15)))
            }
            .accessibilityElement(children: .combine)
            .accessibilityLabel("\(title) column, \(tasks.count) tasks")

            ForEach(tasks) { task in
                KanbanCard(task: task)
                    .contextMenu {
                        Menu("Move to") {
                            ForEach(allColumns.filter { $0 != title }, id: \.self) { col in
                                Button(col.capitalized) { onMove(task, col) }
                            }
                        }
                        Button("Assign column…") { onAssign(task) }
                    }
            }

            if tasks.isEmpty {
                Text("No tasks")
                    .font(.caption)
                    .foregroundStyle(.tertiary)
                    .frame(maxWidth: .infinity, minHeight: 48, alignment: .center)
                    .background(
                        RoundedRectangle(cornerRadius: 8)
                            .strokeBorder(style: StrokeStyle(lineWidth: 1, dash: [4]))
                            .foregroundStyle(Color(.separatorColor))
                    )
            }
        }
        .frame(width: 220)
        .padding(12)
        .background(
            RoundedRectangle(cornerRadius: 12)
                .fill(Color(.windowBackgroundColor))
                .shadow(color: Color(.shadowColor).opacity(0.15), radius: 4, y: 2)
        )
    }
}

private struct KanbanCard: View {
    let task: CalDavTask

    var body: some View {
        VStack(alignment: .leading, spacing: 4) {
            Text(task.summary)
                .font(.subheadline)
                .fontWeight(.medium)
                .lineLimit(2)

            HStack(spacing: 4) {
                if task.priority > 0 {
                    PriorityChip(priority: task.priority)
                }
                if let due = task.due {
                    Text(due, style: .date)
                        .font(.caption2)
                        .foregroundStyle(due < Date() ? .red : .secondary)
                }
            }
        }
        .padding(10)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(
            RoundedRectangle(cornerRadius: 8)
                .fill(Color(.controlBackgroundColor))
                .overlay(
                    RoundedRectangle(cornerRadius: 8)
                        .strokeBorder(Color(.separatorColor), lineWidth: 0.5)
                )
        )
        .accessibilityElement(children: .combine)
        .accessibilityLabel(kanbanCardAccessibilityLabel)
        .accessibilityHint("Activate to move or assign column")
    }

    private var kanbanCardAccessibilityLabel: String {
        var parts = [task.summary]
        if task.priority > 0 {
            let pLabel = task.priority <= 4 ? "high priority" : task.priority == 5 ? "medium priority" : "low priority"
            parts.append(pLabel)
        }
        if let due = task.due {
            parts.append("due \(due.formatted(date: .abbreviated, time: .omitted))")
        }
        return parts.joined(separator: ", ")
    }
}

// ─── CoveyQuadrants ───────────────────────────────────────────────────────────

private struct CoveyQuadrants: View {
    let viewModel: ViewsViewModel

    var body: some View {
        Grid(horizontalSpacing: 12, verticalSpacing: 12) {
            GridRow {
                CoveyQuadrant(
                    label: "Do First",
                    subtitle: "Important & Urgent",
                    color: .red,
                    tasks: viewModel.importantUrgent,
                    onAssign: { viewModel.showAssignModal(for: $0) }
                )
                CoveyQuadrant(
                    label: "Schedule",
                    subtitle: "Important & Not Urgent",
                    color: .blue,
                    tasks: viewModel.importantNotUrgent,
                    onAssign: { viewModel.showAssignModal(for: $0) }
                )
            }
            GridRow {
                CoveyQuadrant(
                    label: "Delegate",
                    subtitle: "Not Important & Urgent",
                    color: .orange,
                    tasks: viewModel.notImportantUrgent,
                    onAssign: { viewModel.showAssignModal(for: $0) }
                )
                CoveyQuadrant(
                    label: "Eliminate",
                    subtitle: "Not Important & Not Urgent",
                    color: .gray,
                    tasks: viewModel.notImportantNotUrgent,
                    onAssign: { viewModel.showAssignModal(for: $0) }
                )
            }
        }
        .padding()
    }
}

private struct CoveyQuadrant: View {
    let label: String
    let subtitle: String
    let color: Color
    let tasks: [CalDavTask]
    let onAssign: (CalDavTask) -> Void

    var body: some View {
        VStack(alignment: .leading, spacing: 6) {
            VStack(alignment: .leading, spacing: 2) {
                Text(label)
                    .font(.headline)
                    .foregroundStyle(color)
                Text(subtitle)
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }

            Divider()

            ScrollView {
                VStack(alignment: .leading, spacing: 4) {
                    ForEach(tasks) { task in
                        Text(task.summary)
                            .font(.caption)
                            .lineLimit(1)
                            .frame(maxWidth: .infinity, alignment: .leading)
                            .contextMenu {
                                Button("Assign column…") { onAssign(task) }
                            }
                            .accessibilityLabel(task.summary)
                            .accessibilityHint("Activate to assign to a kanban column")
                    }
                    if tasks.isEmpty {
                        Text("No tasks")
                            .font(.caption2)
                            .foregroundStyle(.tertiary)
                    }
                }
            }
        }
        .padding(12)
        .frame(maxWidth: .infinity, maxHeight: .infinity)
        .background(
            RoundedRectangle(cornerRadius: 10)
                .fill(color.opacity(0.06))
                .overlay(
                    RoundedRectangle(cornerRadius: 10)
                        .strokeBorder(color.opacity(0.3), lineWidth: 1)
                )
        )
        .accessibilityElement(children: .contain)
        .accessibilityLabel("\(label): \(tasks.count) tasks")
    }
}

// ─── AssignColumnModal ────────────────────────────────────────────────────────

private struct AssignColumnModal: View {
    let task: CalDavTask
    let columns: [String]
    @Binding var selectedColumn: String
    let onConfirm: () -> Void
    let onCancel: () -> Void

    var body: some View {
        NavigationStack {
            Form {
                Section("Task") {
                    Text(task.summary).foregroundStyle(.secondary)
                }
                Section("Move to column") {
                    Picker("Column", selection: $selectedColumn) {
                        ForEach(columns, id: \.self) { col in
                            Text(col.capitalized).tag(col)
                        }
                    }
                    .pickerStyle(.inline)
                }
            }
            .formStyle(.grouped)
            .navigationTitle("Assign Column")
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button("Cancel", action: onCancel)
                }
                ToolbarItem(placement: .confirmationAction) {
                    Button("Move", action: onConfirm)
                        .disabled(selectedColumn.isEmpty)
                }
            }
        }
        .frame(minWidth: 320, minHeight: 260)
    }
}
