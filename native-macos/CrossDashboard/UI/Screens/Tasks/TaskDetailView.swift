import SwiftUI
import CrossDashboardKit

/// Detail pane for a selected task.
/// Shows read-only info; switches to an edit form via the Edit button.
/// Mirrors TaskReadView / TaskDetailView on Android.
struct TaskDetailView: View {

    let taskID: String?

    @Environment(AppViewModel.self) private var appViewModel
    @Environment(\.appContainer) private var container

    @Environment(PomodoroViewModel.self) private var pomodoroVM

    @State private var isEditing: Bool = false
    @State private var editDraft: CalDavTask?
    @State private var isSaving: Bool = false
    @State private var errorMessage: String?

    private var task: CalDavTask? {
        guard let id = taskID else { return nil }
        return container.taskRepository.allTasks.first { $0.uid == id }
    }

    var body: some View {
        Group {
            if let task {
                if isEditing, let draft = editDraft {
                    editForm(draft: draft, original: task)
                } else {
                    readView(task: task)
                }
            } else {
                ContentUnavailableView(
                    "No task selected",
                    systemImage: "checklist",
                    description: Text("Select a task from the list.")
                )
            }
        }
        .onChange(of: taskID) { _, _ in
            isEditing = false
            editDraft = nil
        }
    }

    // ─── Read view ────────────────────────────────────────────────────────────

    private func readView(task: CalDavTask) -> some View {
        PropertyDetailShell(title: task.summary) {
            // Status + priority row
            HStack(spacing: 12) {
                StatusBadge(status: task.status)
                PriorityChip(priority: task.priority)
                Spacer()
                Button {
                    Task {
                        do {
                            _ = try await container.taskRepository.toggleComplete(task)
                        } catch {
                            errorMessage = error.localizedDescription
                        }
                    }
                } label: {
                    Label(
                        task.status == .completed ? "Mark Incomplete" : "Mark Complete",
                        systemImage: task.status == .completed ? "arrow.uturn.backward.circle" : "checkmark.circle"
                    )
                }
                .buttonStyle(.bordered)
                .accessibilityLabel(task.status == .completed ? "Mark incomplete" : "Mark complete")
            }

            // Progress
            if task.percentComplete > 0 {
                VStack(alignment: .leading, spacing: 4) {
                    ReadField(label: "Progress") {
                        HStack {
                            ProgressView(value: Double(task.percentComplete), total: 100)
                                .frame(maxWidth: 200)
                            Text("\(task.percentComplete)%")
                                .font(.caption)
                                .foregroundStyle(.secondary)
                        }
                    }
                }
            }

            // Due date
            if let due = task.due {
                ReadField(label: "Due") {
                    Text(due, format: .dateTime.month().day().year().hour().minute())
                        .foregroundStyle(due < Date() && task.status != .completed ? .red : .primary)
                }
            }

            // Start date
            if let start = task.dtstart {
                ReadField(label: "Starts") {
                    Text(start, format: .dateTime.month().day().year())
                }
            }

            // Categories / tags
            if !task.categories.isEmpty {
                ReadField(label: "Tags") {
                    FlowLayout(spacing: 6) {
                        ForEach(task.categories, id: \.self) { tag in
                            TagChip(tag: tag)
                        }
                    }
                }
            }

            // Description
            if let desc = task.description, !desc.isEmpty {
                ReadMarkdownField(label: "Description", content: desc)
            }

            // Calendar
            if let href = task.calendarHref {
                ReadField(label: "Calendar") {
                    HStack(spacing: 6) {
                        CalendarColorDot(calendarHref: href)
                        Text(calendarDisplayName(href))
                            .font(.callout)
                    }
                }
            }

            // Subtasks
            let subtasks = container.taskRepository.subtasks(of: task.uid)
            if !subtasks.isEmpty {
                ReadField(label: "Subtasks (\(subtasks.count))") {
                    VStack(alignment: .leading, spacing: 4) {
                        ForEach(subtasks) { sub in
                            HStack(spacing: 6) {
                                Image(systemName: sub.status == .completed ? "checkmark.circle.fill" : "circle")
                                    .foregroundStyle(sub.status == .completed ? .green : .secondary)
                                    .font(.caption)
                                Text(sub.summary)
                                    .font(.callout)
                                    .strikethrough(sub.status == .completed)
                            }
                        }
                    }
                }
            }

            // Dates
            ReadField(label: "Created") {
                Text(task.created, format: .dateTime.month().day().year())
                    .foregroundStyle(.secondary)
            }
            ReadField(label: "Modified") {
                Text(task.lastModified, format: .dateTime.month().day().year())
                    .foregroundStyle(.secondary)
            }
        }
        .toolbar {
            ToolbarItem(placement: .secondaryAction) {
                Button {
                    pomodoroVM.start(for: task)
                } label: {
                    Label("Start Pomodoro", systemImage: "timer")
                }
                .accessibilityLabel("Start Pomodoro for this task")
            }
            ToolbarItem(placement: .primaryAction) {
                Button("Edit") {
                    editDraft = task
                    isEditing = true
                }
            }
        }
        .alert("Error", isPresented: Binding(
            get: { errorMessage != nil },
            set: { if !$0 { errorMessage = nil } }
        )) {
            Button("OK") { errorMessage = nil }
        } message: {
            Text(errorMessage ?? "")
        }
    }

    // ─── Edit form ────────────────────────────────────────────────────────────

    private func editForm(draft: CalDavTask, original: CalDavTask) -> some View {
        TaskEditForm(
            task: draft,
            onSave: { updated in
                Task {
                    isSaving = true
                    defer { isSaving = false }
                    do {
                        try await container.taskRepository.update(updated)
                        isEditing = false
                    } catch {
                        errorMessage = error.localizedDescription
                    }
                }
            },
            onCancel: { isEditing = false }
        )
    }

    // ─── Helpers ──────────────────────────────────────────────────────────────

    private func calendarDisplayName(_ href: String) -> String {
        // Attempt to look up from CalDavClient's cached calendar list
        // Fallback: derive from href
        href.components(separatedBy: "/").last ?? href
    }
}

// ─── TaskEditForm ─────────────────────────────────────────────────────────────

private func isTimeEstimateTag(_ tag: String) -> Bool {
    tag.trimmingCharacters(in: .whitespacesAndNewlines)
        .wholeMatch(of: /#?\d+(m|h)/) != nil
}

private func timeEstimateParts(_ categories: [String]) -> (amount: String, unit: String) {
    for tag in categories {
        if let match = tag.trimmingCharacters(in: .whitespacesAndNewlines)
            .wholeMatch(of: /#?(\d+)(m|h)/) {
            return (String(match.1), String(match.2))
        }
    }
    return ("", "m")
}

private struct TaskEditForm: View {

    @State private var summary: String
    @State private var description: String
    @State private var status: TaskStatus
    @State private var priority: Int
    @State private var due: Date?
    @State private var hasDue: Bool
    @State private var categoriesRaw: String
    @State private var estimateAmount: String
    @State private var estimateUnit: String

    private let original: CalDavTask
    let onSave: (CalDavTask) -> Void
    let onCancel: () -> Void

    init(task: CalDavTask, onSave: @escaping (CalDavTask) -> Void, onCancel: @escaping () -> Void) {
        self.original   = task
        self.onSave     = onSave
        self.onCancel   = onCancel
        _summary        = State(initialValue: task.summary)
        _description    = State(initialValue: task.description ?? "")
        _status         = State(initialValue: task.status)
        _priority       = State(initialValue: task.priority)
        _due            = State(initialValue: task.due)
        _hasDue         = State(initialValue: task.due != nil)
        _categoriesRaw  = State(initialValue: task.categories
            .filter { !isTimeEstimateTag($0) }
            .joined(separator: ", "))
        let estimate = timeEstimateParts(task.categories)
        _estimateAmount = State(initialValue: estimate.amount)
        _estimateUnit = State(initialValue: estimate.unit)
    }

    var body: some View {
        Form {
            Section("Summary") {
                TextField("Summary", text: $summary)
            }

            Section("Details") {
                Picker("Status", selection: $status) {
                    ForEach(TaskStatus.allCases, id: \.self) { s in
                        Text(s.displayName).tag(s)
                    }
                }
                Picker("Priority", selection: $priority) {
                    Text("None").tag(0)
                    Text("High").tag(1)
                    Text("Medium").tag(5)
                    Text("Low").tag(9)
                }
                Toggle("Has due date", isOn: $hasDue)
                if hasDue {
                    DatePicker("Due", selection: Binding(
                        get: { due ?? Date() },
                        set: { due = $0 }
                    ), displayedComponents: [.date, .hourAndMinute])
                }
            }

            Section("Tags (comma separated)") {
                TextField("e.g. work, urgent", text: $categoriesRaw)
            }

            Section("Time estimate") {
                HStack {
                    TextField("Amount", text: $estimateAmount)
                        .frame(width: 90)
                    Picker("Unit", selection: $estimateUnit) {
                        Text("Minutes").tag("m")
                        Text("Hours").tag("h")
                    }
                    .pickerStyle(.segmented)
                    if !estimateAmount.isEmpty {
                        Button("Clear") { estimateAmount = "" }
                            .accessibilityLabel("Clear time estimate")
                    }
                }
                Text("Used for the Inbox estimated-time summary.")
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }

            Section("Description") {
                TextEditor(text: $description)
                    .frame(minHeight: 80)
                    .font(.body)
            }
        }
        .formStyle(.grouped)
        .navigationTitle("Edit Task")
        .toolbar {
            ToolbarItem(placement: .cancellationAction) {
                Button("Cancel", role: .cancel, action: onCancel)
            }
            ToolbarItem(placement: .confirmationAction) {
                Button("Save") { save() }
                    .disabled(summary.trimmingCharacters(in: .whitespaces).isEmpty)
            }
        }
    }

    private func save() {
        var cats = categoriesRaw
            .split(separator: ",")
            .map { $0.trimmingCharacters(in: .whitespaces) }
            .filter { !$0.isEmpty && !isTimeEstimateTag($0) }
        let digits = estimateAmount.filter { $0.isNumber }
        if let amount = Int(digits), amount > 0 {
            cats.append("\(amount)\(estimateUnit)")
        }

        let updated = CalDavTask(
            uid: original.uid,
            summary: summary.trimmingCharacters(in: .whitespaces),
            description: description.isEmpty ? nil : description,
            status: status,
            priority: priority,
            percentComplete: status == .completed ? 100 : original.percentComplete,
            due: hasDue ? due : nil,
            dtstart: original.dtstart,
            completed: status == .completed ? (original.completed ?? Date()) : nil,
            created: original.created,
            lastModified: Date(),
            categories: cats,
            location: original.location,
            parentUid: original.parentUid,
            calendarHref: original.calendarHref,
            etag: original.etag,
            href: original.href
        )
        onSave(updated)
    }
}

// ─── TaskStatus display name ──────────────────────────────────────────────────

private extension TaskStatus {
    var displayName: String {
        switch self {
        case .needsAction: return "Needs Action"
        case .inProcess:   return "In Progress"
        case .completed:   return "Completed"
        case .cancelled:   return "Cancelled"
        }
    }
}

// ─── FlowLayout (wrapping HStack for tags) ────────────────────────────────────

struct FlowLayout: Layout {
    var spacing: CGFloat = 8

    func sizeThatFits(proposal: ProposedViewSize, subviews: Subviews, cache: inout ()) -> CGSize {
        let rows = computeRows(proposal: proposal, subviews: subviews)
        let height = rows.map { $0.map { $0.size.height }.max() ?? 0 }.reduce(0, +) + spacing * CGFloat(max(rows.count - 1, 0))
        return CGSize(width: proposal.width ?? 0, height: height)
    }

    func placeSubviews(in bounds: CGRect, proposal: ProposedViewSize, subviews: Subviews, cache: inout ()) {
        let rows = computeRows(proposal: proposal, subviews: subviews)
        var y = bounds.minY
        for row in rows {
            var x = bounds.minX
            let rowHeight = row.map { $0.size.height }.max() ?? 0
            for item in row {
                item.view.place(at: CGPoint(x: x, y: y), anchor: .topLeading, proposal: .unspecified)
                x += item.size.width + spacing
            }
            y += rowHeight + spacing
        }
    }

    private func computeRows(proposal: ProposedViewSize, subviews: Subviews) -> [[(view: LayoutSubview, size: CGSize)]] {
        let maxWidth = proposal.width ?? .infinity
        var rows: [[(view: LayoutSubview, size: CGSize)]] = [[]]
        var rowWidth: CGFloat = 0

        for view in subviews {
            let size = view.sizeThatFits(.unspecified)
            if rowWidth + size.width > maxWidth, !rows[rows.endIndex - 1].isEmpty {
                rows.append([])
                rowWidth = 0
            }
            rows[rows.endIndex - 1].append((view, size))
            rowWidth += size.width + spacing
        }
        return rows
    }
}
