import SwiftUI
import CrossDashboardKit

/// Unified inbox showing upcoming events, due tasks, and open issues.
/// Mirrors InboxScreen on Android.
struct InboxView: View {

    @Environment(AppViewModel.self) private var appViewModel
    @State private var viewModel = InboxViewModel()

    var body: some View {
        @Bindable var vm = viewModel
        VStack(spacing: 0) {
            Group {
                if viewModel.isLoading && viewModel.allItems.isEmpty {
                    ProgressView("Loading…")
                        .frame(maxWidth: .infinity, maxHeight: .infinity)
                } else if viewModel.filteredItems.isEmpty {
                    ContentUnavailableView(
                        viewModel.searchText.isEmpty ? "Inbox empty" : "No results",
                        systemImage: "tray",
                        description: Text(
                            viewModel.searchText.isEmpty
                                ? "No upcoming events, tasks, or issues."
                                : "Nothing matches '\(viewModel.searchText)'."
                        )
                    )
                } else {
                    itemList
                }
            }

            // Total estimated time footer
            if !viewModel.totalEstimatedLabel.isEmpty {
                Divider()
                HStack {
                    Label("Total estimated", systemImage: "clock")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                    Spacer()
                    Text(viewModel.totalEstimatedLabel)
                        .font(.caption)
                        .fontWeight(.medium)
                        .foregroundStyle(.primary)
                }
                .padding(.horizontal, 12)
                .padding(.vertical, 6)
                .background(.regularMaterial)
                .accessibilityElement(children: .combine)
                .accessibilityLabel("Total estimated time: \(viewModel.totalEstimatedLabel)")
            }
        }
        .navigationTitle("Inbox")
        .searchable(text: $vm.searchText, prompt: "Search inbox")
        .toolbar { toolbarContent }
    }

    // ─── Item list ────────────────────────────────────────────────────────────

    private var itemList: some View {
        List {
            ForEach(viewModel.filteredItems) { item in
                InboxItemRow(item: item, onOpen: navigationAction(for: item))
                    .listRowSeparator(.visible)
            }
        }
        .listStyle(.inset)
    }

    private func navigationAction(for item: InboxItem) -> (() -> Void)? {
        switch item {
        case .event(let event, _):
            return { appViewModel.openEvent(event.uid) }
        case .task(let task, _):
            return { appViewModel.openTask(task.uid) }
        case .issue(let issue, _):
            return { appViewModel.openIssue(issue.id) }
        case .milestone:
            return nil
        }
    }

    // ─── Toolbar ──────────────────────────────────────────────────────────────

    @ToolbarContentBuilder
    private var toolbarContent: some ToolbarContent {
        ToolbarItem(placement: .primaryAction) {
            Picker("Filter", selection: Binding(
                get: { viewModel.filter },
                set: { viewModel.filter = $0 }
            )) {
                ForEach(InboxViewModel.ItemType.allCases) { type in
                    Text(type.rawValue).tag(type)
                }
            }
            .pickerStyle(.menu)
            .accessibilityLabel("Filter inbox items")
        }
        ToolbarItem {
            Button {
                Task { await viewModel.sync() }
            } label: {
                Label("Sync", systemImage: "arrow.clockwise")
            }
            .accessibilityLabel("Sync inbox")
        }
    }
}

// ─── InboxItemRow ─────────────────────────────────────────────────────────────

private struct InboxItemRow: View {
    let item: InboxItem
    let onOpen: (() -> Void)?

    var body: some View {
        Group {
            if let onOpen {
                Button(action: onOpen) {
                    rowContent
                }
                .buttonStyle(.plain)
            } else {
                rowContent
            }
        }
        .accessibilityElement(children: .combine)
        .accessibilityLabel(itemTitle)
        .accessibilityValue(itemSubtitle)
        .accessibilityHint(accessibilityHint)
    }

    private var rowContent: some View {
        HStack(spacing: 12) {
            itemIcon
                .frame(width: 28, height: 28)

            VStack(alignment: .leading, spacing: 2) {
                Text(itemTitle)
                    .fontWeight(.medium)
                    .lineLimit(1)
                Text(itemSubtitle)
                    .font(.caption)
                    .foregroundStyle(.secondary)
                    .lineLimit(1)
            }

            Spacer()

            if let est = estimatedLabel {
                Text(est)
                    .font(.caption2)
                    .foregroundStyle(.tertiary)
                    .padding(.horizontal, 6)
                    .padding(.vertical, 2)
                    .background(Capsule().fill(Color.accentColor.opacity(0.1)))
            }
        }
        .padding(.vertical, 4)
        .contentShape(Rectangle())
    }

    private var accessibilityHint: String {
        let estimate = estimatedLabel.map { " Estimated \($0)." } ?? ""
        return onOpen == nil ? estimate.trimmingCharacters(in: .whitespaces) : "Open item.\(estimate)"
    }

    @ViewBuilder
    private var itemIcon: some View {
        switch item {
        case .event:
            Image(systemName: "calendar")
                .foregroundStyle(.blue)
        case .task(let t, _):
            Image(systemName: t.status == .completed ? "checkmark.circle.fill" : "circle")
                .foregroundStyle(t.status == .completed ? .green : .primary)
        case .issue(let i, _):
            Image(systemName: i.state == "open" ? "exclamationmark.bubble" : "checkmark.bubble")
                .foregroundStyle(i.state == "open" ? .orange : .purple)
        case .milestone:
            Image(systemName: "flag")
                .foregroundStyle(.teal)
        }
    }

    private var itemTitle: String {
        switch item {
        case .event(let e, _):  return e.summary
        case .task(let t, _):   return t.summary
        case .issue(let i, _):  return i.title
        case .milestone(let m): return m.title
        }
    }

    private var itemSubtitle: String {
        switch item {
        case .event(let e, _):
            return e.start.formatted(date: .abbreviated, time: .shortened)
        case .task(let t, _):
            if let due = t.due {
                let overdue = due < Date()
                return overdue ? "Overdue · \(due.formatted(date: .abbreviated, time: .omitted))"
                               : "Due \(due.formatted(date: .abbreviated, time: .omitted))"
            }
            return t.calendarHref ?? ""
        case .issue(let i, _):
            return "\(i.repository) #\(i.number)"
        case .milestone(let m):
            if let due = m.dueOn {
                return "Due \(due.formatted(date: .abbreviated, time: .omitted)) · \(m.openIssues) open"
            }
            return "\(m.openIssues) open issues"
        }
    }

    private var estimatedLabel: String? {
        switch item {
        case .event(_, let d):
            let h = d / 60; let m = d % 60
            if d == 0 { return nil }
            return h > 0 ? (m == 0 ? "\(h)h" : "\(h)h\(m)m") : "\(m)m"
        case .task(_, let m):
            guard let m, m > 0 else { return nil }
            let h = m / 60; let rem = m % 60
            return h > 0 ? (rem == 0 ? "\(h)h" : "\(h)h\(rem)m") : "\(rem)m"
        case .issue(_, let m):
            guard let m, m > 0 else { return nil }
            let h = m / 60; let rem = m % 60
            return h > 0 ? (rem == 0 ? "\(h)h" : "\(h)h\(rem)m") : "\(rem)m"
        case .milestone:
            return nil
        }
    }
}
