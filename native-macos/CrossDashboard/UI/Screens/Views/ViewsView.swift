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
            ToolbarItemGroup(placement: .primaryAction) {
                // Kanban column config (only shown in kanban mode)
                if viewModel.mode == .kanban {
                    Button {
                        viewModel.openColumnConfig()
                    } label: {
                        Label("Configure columns", systemImage: "slider.horizontal.3")
                    }
                    .help("Add, rename, or remove Kanban columns")
                    .keyboardShortcut(",", modifiers: [.command, .shift])
                }

                Picker("View mode", selection: $vm.mode) {
                    ForEach(ViewsViewModel.ViewMode.allCases) { m in
                        Label(m.rawValue, systemImage: m == .kanban ? "rectangle.split.3x1" : "square.grid.2x2")
                            .tag(m)
                    }
                }
                .pickerStyle(.segmented)
                .accessibilityLabel("Switch between Kanban and Covey views")
            }
        }
        // Assign single task to a column
        .sheet(item: $vm.assigningTask) { task in
            AssignColumnModal(
                task: task,
                columns: viewModel.kanbanColumns,
                selectedColumn: $vm.assignColumnInput,
                onConfirm: { viewModel.confirmAssign() },
                onCancel: { viewModel.assigningTask = nil }
            )
        }
        // Bulk assign: add open tasks to a column
        .sheet(isPresented: Binding(
            get: { viewModel.bulkAssignTarget != nil },
            set: { if !$0 { viewModel.closeBulkAssign() } }
        )) {
            if let target = viewModel.bulkAssignTarget {
                BulkAssignSheet(
                    target: target,
                    availableColumns: viewModel.kanbanColumns,
                    viewModel: viewModel
                )
            }
        }
        // Column config: add / rename / remove columns
        .sheet(isPresented: Binding(
            get: { viewModel.editingColumns != nil },
            set: { if !$0 { viewModel.closeColumnConfig() } }
        )) {
            if let cols = viewModel.editingColumns {
                ColumnConfigSheet(
                    columns: cols,
                    onSave: { viewModel.saveColumns($0) },
                    onCancel: { viewModel.closeColumnConfig() }
                )
            }
        }
    }
}

// ─── KanbanBoard ──────────────────────────────────────────────────────────────

private struct KanbanBoard: View {
    let viewModel: ViewsViewModel

    var body: some View {
        ScrollView(.horizontal, showsIndicators: false) {
            HStack(alignment: .top, spacing: 16) {
                // Unassigned column — tasks that don't match any configured column
                let unassigned = viewModel.unassignedTasks
                if !unassigned.isEmpty {
                    KanbanColumn(
                        title: "Unassigned",
                        tasks: unassigned,
                        isUnassigned: true,
                        allColumns: viewModel.kanbanColumns,
                        onMove: { task, col in viewModel.moveTask(task, toColumn: col) },
                        onAssign: { viewModel.showAssignModal(for: $0) },
                        onAddTasks: nil
                    )
                }

                ForEach(viewModel.kanbanColumns, id: \.self) { column in
                    KanbanColumn(
                        title: column,
                        tasks: viewModel.tasks(inColumn: column),
                        isUnassigned: false,
                        allColumns: viewModel.kanbanColumns,
                        onMove: { task, col in viewModel.moveTask(task, toColumn: col) },
                        onAssign: { viewModel.showAssignModal(for: $0) },
                        onAddTasks: { viewModel.openBulkAssign(targetColumn: column) }
                    )
                }
            }
            .padding()
            .frame(minHeight: 0, maxHeight: .infinity, alignment: .top)
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity)
    }
}

private struct KanbanColumn: View {
    let title: String
    let tasks: [CalDavTask]
    let isUnassigned: Bool
    let allColumns: [String]
    let onMove: (CalDavTask, String) -> Void
    let onAssign: (CalDavTask) -> Void
    let onAddTasks: (() -> Void)?

    var body: some View {
        VStack(alignment: .leading, spacing: 0) {
            // ── Column header ──────────────────────────────────────────────
            HStack(spacing: 6) {
                Text(title.capitalized)
                    .font(.headline)
                    .fontWeight(.semibold)
                Text("\(tasks.count)")
                    .font(.caption)
                    .foregroundStyle(.secondary)
                    .padding(.horizontal, 6)
                    .padding(.vertical, 2)
                    .background(Capsule().fill(Color.accentColor.opacity(0.15)))
                Spacer()
                if let addAction = onAddTasks {
                    Button(action: addAction) {
                        Image(systemName: "plus")
                            .font(.caption.weight(.semibold))
                    }
                    .buttonStyle(.plain)
                    .frame(width: 24, height: 24)
                    .background(Color(.controlBackgroundColor))
                    .clipShape(Circle())
                    .help("Add open tasks to \(title.capitalized)")
                    .accessibilityLabel("Add tasks to \(title.capitalized) column")
                }
            }
            .padding(.horizontal, 12)
            .padding(.vertical, 10)
            .background(Color(.windowBackgroundColor))
            .accessibilityElement(children: .combine)
            .accessibilityLabel("\(title) column, \(tasks.count) tasks")

            Divider()

            // ── Card list ──────────────────────────────────────────────────
            ScrollView {
                VStack(spacing: 6) {
                    ForEach(tasks) { task in
                        KanbanCard(
                            task: task,
                            currentColumn: isUnassigned ? nil : title,
                            allColumns: allColumns,
                            onMove: { col in onMove(task, col) },
                            onAssign: { onAssign(task) }
                        )
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
                            .padding(8)
                    }
                }
                .padding(8)
            }
        }
        .frame(width: 220)
        .background(
            RoundedRectangle(cornerRadius: 12)
                .fill(Color(.controlBackgroundColor).opacity(0.5))
                .overlay(
                    RoundedRectangle(cornerRadius: 12)
                        .strokeBorder(Color(.separatorColor), lineWidth: 1)
                )
        )
    }
}

private struct KanbanCard: View {
    let task: CalDavTask
    let currentColumn: String?
    let allColumns: [String]
    let onMove: (String) -> Void
    let onAssign: () -> Void

    var body: some View {
        Button(action: onAssign) {
            VStack(alignment: .leading, spacing: 4) {
                Text(task.summary)
                    .font(.subheadline)
                    .fontWeight(.medium)
                    .lineLimit(2)
                    .multilineTextAlignment(.leading)
                    .foregroundStyle(Color(.labelColor))

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
                    .fill(Color(.windowBackgroundColor))
                    .overlay(
                        RoundedRectangle(cornerRadius: 8)
                            .strokeBorder(Color(.separatorColor), lineWidth: 0.5)
                    )
            )
        }
        .buttonStyle(.plain)
        .contextMenu {
            Button("Assign to column…") { onAssign() }
            if !allColumns.isEmpty {
                Divider()
                Menu("Move to") {
                    ForEach(allColumns.filter {
                        $0.lowercased() != (currentColumn ?? "").lowercased()
                    }, id: \.self) { col in
                        Button(col.capitalized) { onMove(col) }
                    }
                }
            }
        }
        .accessibilityElement(children: .combine)
        .accessibilityLabel(kanbanCardAccessibilityLabel)
        .accessibilityHint("Click to assign to a different column")
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
                        Button { onAssign(task) } label: {
                            HStack(spacing: 6) {
                                Image(systemName: "square")
                                    .font(.caption2)
                                    .foregroundStyle(.secondary)
                                Text(task.summary)
                                    .font(.caption)
                                    .lineLimit(2)
                                    .multilineTextAlignment(.leading)
                                    .foregroundStyle(Color(.labelColor))
                                Spacer()
                            }
                            .padding(.horizontal, 8)
                            .padding(.vertical, 5)
                            .background(
                                RoundedRectangle(cornerRadius: 6)
                                    .fill(color.opacity(0.08))
                            )
                        }
                        .buttonStyle(.plain)
                        .contextMenu {
                            Button("Assign to column…") { onAssign(task) }
                        }
                        .accessibilityLabel(task.summary)
                        .accessibilityHint("Click to assign to a kanban column")
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

// ─── BulkAssignSheet ──────────────────────────────────────────────────────────

private struct BulkAssignSheet: View {
    let target: String
    let availableColumns: [String]
    let viewModel: ViewsViewModel

    @Environment(\.dismiss) private var dismiss

    var body: some View {
        NavigationStack {
            VStack(spacing: 0) {
                // Column selector
                ScrollView(.horizontal, showsIndicators: false) {
                    HStack(spacing: 8) {
                        ForEach(availableColumns, id: \.self) { col in
                            Button {
                                viewModel.setBulkAssignTarget(col)
                            } label: {
                                Text(col.capitalized)
                                    .font(.subheadline.weight(.medium))
                                    .padding(.horizontal, 14)
                                    .padding(.vertical, 6)
                                    .background(
                                        Capsule().fill(
                                            col.lowercased() == target.lowercased()
                                                ? Color.accentColor
                                                : Color(.controlBackgroundColor)
                                        )
                                    )
                                    .foregroundStyle(
                                        col.lowercased() == target.lowercased()
                                            ? Color.white
                                            : Color(.labelColor)
                                    )
                            }
                            .buttonStyle(.plain)
                            .accessibilityLabel(col.capitalized)
                            .accessibilityAddTraits(col.lowercased() == target.lowercased() ? .isSelected : [])
                        }
                    }
                    .padding(.horizontal, 16)
                    .padding(.vertical, 12)
                }
                .background(Color(.windowBackgroundColor))

                Divider()

                let assignable = viewModel.tasksNotIn(column: target)
                if assignable.isEmpty {
                    ContentUnavailableView(
                        "All tasks assigned",
                        systemImage: "checkmark.circle",
                        description: Text("Every open task is already in \(target.capitalized).")
                    )
                    .frame(maxWidth: .infinity, maxHeight: .infinity)
                } else {
                    List(assignable) { task in
                        BulkAssignRow(task: task) {
                            viewModel.assignFromBulk(task, toColumn: target)
                        }
                    }
                    .listStyle(.plain)
                }
            }
            .navigationTitle("Add tasks to \(target.capitalized)")
            .toolbar {
                ToolbarItem(placement: .confirmationAction) {
                    Button("Done") { dismiss() }
                }
            }
        }
        .frame(minWidth: 380, minHeight: 420)
    }
}

private struct BulkAssignRow: View {
    let task: CalDavTask
    let onAdd: () -> Void

    var body: some View {
        HStack(spacing: 10) {
            Image(systemName: "square")
                .font(.caption)
                .foregroundStyle(.secondary)
            VStack(alignment: .leading, spacing: 2) {
                Text(task.summary)
                    .font(.subheadline)
                    .lineLimit(2)
                HStack(spacing: 6) {
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
            Spacer()
            Button(action: onAdd) {
                Image(systemName: "plus.circle.fill")
                    .font(.title3)
                    .foregroundStyle(Color.accentColor)
            }
            .buttonStyle(.plain)
            .help("Add to column")
            .accessibilityLabel("Add \(task.summary) to column")
        }
        .padding(.vertical, 4)
        .accessibilityElement(children: .combine)
        .accessibilityLabel(task.summary)
        .accessibilityHint("Click plus to assign to this column")
    }
}

// ─── ColumnConfigSheet ────────────────────────────────────────────────────────

private struct ColumnConfigSheet: View {
    @State private var columns: [String]
    @State private var newColumnName: String = ""
    @State private var editingIndex: Int? = nil
    @State private var editingText: String = ""
    @FocusState private var isNewFieldFocused: Bool

    let onSave: ([String]) -> Void
    let onCancel: () -> Void

    init(columns: [String], onSave: @escaping ([String]) -> Void, onCancel: @escaping () -> Void) {
        _columns = State(initialValue: columns)
        self.onSave = onSave
        self.onCancel = onCancel
    }

    var body: some View {
        NavigationStack {
            List {
                Section("Current columns") {
                    ForEach(Array(columns.enumerated()), id: \.offset) { index, col in
                        HStack {
                            if editingIndex == index {
                                TextField("Column name", text: $editingText)
                                    .textFieldStyle(.roundedBorder)
                                    .onSubmit { commitRename(at: index) }
                                Button("Done") { commitRename(at: index) }
                                    .buttonStyle(.borderedProminent)
                                    .controlSize(.small)
                            } else {
                                Text(col.capitalized)
                                    .font(.body)
                                Spacer()
                                Button {
                                    editingIndex = index
                                    editingText = col
                                } label: {
                                    Image(systemName: "pencil")
                                        .foregroundStyle(.secondary)
                                }
                                .buttonStyle(.plain)
                                .help("Rename \(col.capitalized)")
                                .accessibilityLabel("Rename \(col.capitalized)")
                            }
                        }
                        .padding(.vertical, 2)
                    }
                    .onDelete { offsets in
                        columns.remove(atOffsets: offsets)
                        editingIndex = nil
                    }
                    .onMove { from, to in
                        columns.move(fromOffsets: from, toOffset: to)
                    }
                }

                Section("Add column") {
                    HStack {
                        TextField("New column name", text: $newColumnName)
                            .textFieldStyle(.roundedBorder)
                            .focused($isNewFieldFocused)
                            .onSubmit { addColumn() }
                        Button("Add", action: addColumn)
                            .buttonStyle(.borderedProminent)
                            .controlSize(.small)
                            .disabled(newColumnName.trimmingCharacters(in: .whitespaces).isEmpty)
                    }
                }
            }
            .listStyle(.inset)
            .navigationTitle("Kanban Columns")
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button("Cancel", action: onCancel)
                }
                ToolbarItem(placement: .confirmationAction) {
                    Button("Save") { onSave(columns) }
                        .disabled(columns.isEmpty)
                }
            }
        }
        .frame(minWidth: 340, minHeight: 360)
    }

    private func addColumn() {
        let trimmed = newColumnName.trimmingCharacters(in: .whitespaces)
        guard !trimmed.isEmpty else { return }
        columns.append(trimmed.lowercased())
        newColumnName = ""
    }

    private func commitRename(at index: Int) {
        let trimmed = editingText.trimmingCharacters(in: .whitespaces)
        if !trimmed.isEmpty {
            columns[index] = trimmed.lowercased()
        }
        editingIndex = nil
        editingText = ""
    }
}
