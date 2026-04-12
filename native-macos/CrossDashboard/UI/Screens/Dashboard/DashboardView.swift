import SwiftUI
import Charts
import CrossDashboardKit

struct DashboardView: View {

    @Environment(\.appContainer) private var container
    @State private var viewModel = DashboardViewModel()

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 24) {
                statsCard
                HStack(alignment: .top, spacing: 20) {
                    upcomingEventsCard
                    dueSoonCard
                }
                issuesCard
            }
            .padding(20)
        }
        .navigationTitle("Dashboard")
        .toolbar {
            ToolbarItem(placement: .primaryAction) {
                if viewModel.isLoading {
                    ProgressView().controlSize(.small)
                }
            }
        }
        .task {
            await viewModel.load()
        }
        .refreshable {
            await container.syncAll()
            await viewModel.load()
        }
    }

    // ─── 7-day Stats card ─────────────────────────────────────────────────────

    private var statsCard: some View {
        GroupBox {
            VStack(alignment: .leading, spacing: 12) {
                HStack {
                    statPill(value: viewModel.totalTasksThisWeek,    label: "Tasks done",    color: .green)
                    statPill(value: viewModel.totalPomodorosThisWeek, label: "Pomodoros",     color: .orange)
                    statPill(value: viewModel.openIssueCount,         label: "Open issues",   color: .blue)
                    Spacer()
                }

                Chart(viewModel.chartData, id: \.label) { item in
                    BarMark(
                        x: .value("Day", item.label),
                        y: .value("Tasks", item.tasks)
                    )
                    .foregroundStyle(Color.green.opacity(0.7))

                    BarMark(
                        x: .value("Day", item.label),
                        y: .value("Pomodoros", item.pomodoros)
                    )
                    .foregroundStyle(Color.orange.opacity(0.7))
                }
                .chartLegend(position: .topTrailing)
                .frame(height: 120)
            }
        } label: {
            Label("This week", systemImage: "chart.bar.fill")
                .font(.headline)
        }
    }

    // ─── Upcoming events ──────────────────────────────────────────────────────

    private var upcomingEventsCard: some View {
        GroupBox {
            if viewModel.upcomingEvents.isEmpty {
                emptyState(icon: "calendar", message: "No upcoming events")
            } else {
                VStack(spacing: 0) {
                    ForEach(viewModel.upcomingEvents) { event in
                        eventRow(event)
                        if event.id != viewModel.upcomingEvents.last?.id {
                            Divider()
                        }
                    }
                }
            }
        } label: {
            Label("Upcoming Events", systemImage: "calendar")
                .font(.headline)
        }
        .frame(maxWidth: .infinity)
    }

    // ─── Tasks due soon ───────────────────────────────────────────────────────

    private var dueSoonCard: some View {
        GroupBox {
            if viewModel.dueSoonTasks.isEmpty {
                emptyState(icon: "checklist", message: "No tasks due soon")
            } else {
                VStack(spacing: 0) {
                    ForEach(viewModel.dueSoonTasks) { task in
                        taskRow(task)
                        if task.id != viewModel.dueSoonTasks.last?.id {
                            Divider()
                        }
                    }
                }
            }
        } label: {
            Label("Due Soon", systemImage: "checklist")
                .font(.headline)
        }
        .frame(maxWidth: .infinity)
    }

    // ─── Issues count ─────────────────────────────────────────────────────────

    private var issuesCard: some View {
        GroupBox {
            HStack {
                Image(systemName: "exclamationmark.bubble.fill")
                    .foregroundStyle(.blue)
                    .font(.title2)
                VStack(alignment: .leading) {
                    Text("\(viewModel.openIssueCount)")
                        .font(.title.monospacedDigit())
                        .fontWeight(.bold)
                    Text("Open issues across all repositories")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }
                Spacer()
            }
            .padding(.vertical, 4)
        } label: {
            Label("Gitea Issues", systemImage: "exclamationmark.bubble")
                .font(.headline)
        }
    }

    // ─── Row helpers ──────────────────────────────────────────────────────────

    private func eventRow(_ event: CalendarEvent) -> some View {
        HStack(spacing: 8) {
            CalendarColorDot(calendarHref: event.calendarHref)
            VStack(alignment: .leading, spacing: 2) {
                Text(event.summary)
                    .lineLimit(1)
                    .font(.callout)
                Text(event.start, style: .relative)
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }
            Spacer()
        }
        .padding(.vertical, 6)
        .accessibilityElement(children: .combine)
        .accessibilityLabel("\(event.summary), \(event.start.formatted(date: .abbreviated, time: .shortened))")
    }

    private func taskRow(_ task: CalDavTask) -> some View {
        HStack(spacing: 8) {
            CalendarColorDot(calendarHref: task.calendarHref)
            VStack(alignment: .leading, spacing: 2) {
                Text(task.summary)
                    .lineLimit(1)
                    .font(.callout)
                if let due = task.due {
                    Text(due, style: .relative)
                        .font(.caption)
                        .foregroundStyle(due < Date() ? .red : .secondary)
                }
            }
            Spacer()
            PriorityChip(priority: task.priority)
        }
        .padding(.vertical, 6)
        .accessibilityElement(children: .combine)
        .accessibilityLabel("\(task.summary)\(task.due.map { ", due \($0.formatted(date: .abbreviated, time: .omitted))" } ?? "")")
    }

    private func statPill(value: Int, label: String, color: Color) -> some View {
        VStack(spacing: 2) {
            Text("\(value)")
                .font(.title2.monospacedDigit().bold())
                .foregroundStyle(color)
            Text(label)
                .font(.caption2)
                .foregroundStyle(.secondary)
        }
        .frame(minWidth: 72)
        .padding(.horizontal, 8)
        .padding(.vertical, 6)
        .background(color.opacity(0.1), in: RoundedRectangle(cornerRadius: 8))
    }

    private func emptyState(icon: String, message: String) -> some View {
        HStack {
            Spacer()
            VStack(spacing: 6) {
                Image(systemName: icon)
                    .font(.title3)
                    .foregroundStyle(.tertiary)
                Text(message)
                    .font(.caption)
                    .foregroundStyle(.tertiary)
            }
            .padding(.vertical, 16)
            Spacer()
        }
    }
}
