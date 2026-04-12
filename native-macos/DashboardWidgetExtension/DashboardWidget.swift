import WidgetKit
import SwiftUI
import SwiftData
import CrossDashboardKit

// ─── Entry ────────────────────────────────────────────────────────────────────

struct DashboardWidgetEntry: TimelineEntry {
    let date: Date
    let upcomingEvents: [WidgetEvent]
    let dueSoonTasks: [WidgetTask]
    let openIssuesCount: Int
}

struct WidgetEvent: Identifiable {
    let id: String
    let summary: String
    let start: Date
    let calendarColor: String?
}

struct WidgetTask: Identifiable {
    let id: String
    let summary: String
    let due: Date?
    let isOverdue: Bool
    let priority: Int
}

// ─── Timeline Provider ────────────────────────────────────────────────────────

struct DashboardWidgetProvider: TimelineProvider {

    private let appGroupID = "group.com.crossdashboard"

    func placeholder(in context: Context) -> DashboardWidgetEntry {
        .placeholder
    }

    func getSnapshot(in context: Context, completion: @escaping (DashboardWidgetEntry) -> Void) {
        if context.isPreview {
            completion(.placeholder)
        } else {
            completion(loadEntry())
        }
    }

    func getTimeline(in context: Context, completion: @escaping (Timeline<DashboardWidgetEntry>) -> Void) {
        let entry = loadEntry()
        // Refresh in 30 min to pick up sync changes
        let nextRefresh = Calendar.current.date(byAdding: .minute, value: 30, to: Date()) ?? Date()
        completion(Timeline(entries: [entry], policy: .after(nextRefresh)))
    }

    // ─── Data loading ─────────────────────────────────────────────────────────

    private func loadEntry() -> DashboardWidgetEntry {
        guard let container = makeContainer() else {
            return DashboardWidgetEntry(date: Date(), upcomingEvents: [], dueSoonTasks: [], openIssuesCount: 0)
        }
        let ctx = ModelContext(container)

        let now = Date()
        let weekFromNow = Calendar.current.date(byAdding: .day, value: 7, to: now) ?? now

        // Upcoming events (next 7 days, up to 5)
        let eventDescriptor = FetchDescriptor<EventModel>(
            predicate: #Predicate { $0.startEpoch >= now.timeIntervalSince1970 && $0.startEpoch <= weekFromNow.timeIntervalSince1970 },
            sortBy: [SortDescriptor(\.startEpoch)]
        )
        let eventModels = (try? ctx.fetch(eventDescriptor)) ?? []
        let events = eventModels.prefix(5).map { m in
            WidgetEvent(
                id: m.uid,
                summary: m.summary,
                start: Date(timeIntervalSince1970: m.startEpoch),
                calendarColor: nil
            )
        }

        // Due-soon tasks (next 7 days, not completed, up to 5)
        let taskDescriptor = FetchDescriptor<TaskModel>(
            predicate: #Predicate {
                $0.statusRaw != "completed" &&
                $0.dueEpoch != nil &&
                ($0.dueEpoch ?? 0) <= weekFromNow.timeIntervalSince1970
            },
            sortBy: [SortDescriptor(\.dueEpoch)]
        )
        let taskModels = (try? ctx.fetch(taskDescriptor)) ?? []
        let tasks = taskModels.prefix(5).map { m in
            WidgetTask(
                id: m.uid,
                summary: m.summary,
                due: m.dueEpoch.map { Date(timeIntervalSince1970: $0) },
                isOverdue: (m.dueEpoch ?? Double.infinity) < now.timeIntervalSince1970,
                priority: m.priority
            )
        }

        // Open issues count
        let issueDescriptor = FetchDescriptor<IssueModel>(
            predicate: #Predicate { $0.state == "open" }
        )
        let openIssuesCount = (try? ctx.fetchCount(issueDescriptor)) ?? 0

        return DashboardWidgetEntry(
            date: now,
            upcomingEvents: Array(events),
            dueSoonTasks: Array(tasks),
            openIssuesCount: openIssuesCount
        )
    }

    private func makeContainer() -> ModelContainer? {
        let schema = Schema([EventModel.self, TaskModel.self, NoteModel.self, IssueModel.self, StatsModel.self])
        let storeURL = PersistenceController.groupContainerURL(appGroupID: appGroupID)
        guard let storeURL else { return try? ModelContainer(for: schema) }
        let config = ModelConfiguration(schema: schema, url: storeURL)
        return try? ModelContainer(for: schema, configurations: [config])
    }
}

// ─── Entry placeholder ────────────────────────────────────────────────────────

extension DashboardWidgetEntry {
    static let placeholder = DashboardWidgetEntry(
        date: Date(),
        upcomingEvents: [
            WidgetEvent(id: "1", summary: "Team standup", start: Date().addingTimeInterval(3600), calendarColor: nil),
            WidgetEvent(id: "2", summary: "Design review", start: Date().addingTimeInterval(7200), calendarColor: nil),
        ],
        dueSoonTasks: [
            WidgetTask(id: "a", summary: "Write release notes", due: Date().addingTimeInterval(86400), isOverdue: false, priority: 1),
            WidgetTask(id: "b", summary: "Fix CI flake", due: Date().addingTimeInterval(-3600), isOverdue: true, priority: 5),
        ],
        openIssuesCount: 4
    )
}

// ─── Widget Views ─────────────────────────────────────────────────────────────

struct DashboardWidgetEntryView: View {
    @Environment(\.widgetFamily) private var family
    var entry: DashboardWidgetEntry

    var body: some View {
        switch family {
        case .systemSmall:
            SmallWidgetView(entry: entry)
        case .systemMedium:
            MediumWidgetView(entry: entry)
        case .systemLarge:
            LargeWidgetView(entry: entry)
        default:
            SmallWidgetView(entry: entry)
        }
    }
}

// ─── Small: next event + tasks count ─────────────────────────────────────────

private struct SmallWidgetView: View {
    let entry: DashboardWidgetEntry

    var body: some View {
        VStack(alignment: .leading, spacing: 6) {
            Label("CrossDashboard", systemImage: "square.grid.2x2.fill")
                .font(.caption2.weight(.semibold))
                .foregroundStyle(.secondary)

            Spacer()

            if let next = entry.upcomingEvents.first {
                VStack(alignment: .leading, spacing: 2) {
                    Text(next.summary)
                        .font(.caption.weight(.medium))
                        .lineLimit(2)
                    Text(next.start, style: .relative)
                        .font(.caption2)
                        .foregroundStyle(.secondary)
                }
            } else {
                Text("No upcoming events")
                    .font(.caption2)
                    .foregroundStyle(.secondary)
            }

            Spacer()

            HStack(spacing: 8) {
                Label("\(entry.dueSoonTasks.count)", systemImage: "checklist")
                    .font(.caption2)
                    .foregroundStyle(.primary)
                if entry.openIssuesCount > 0 {
                    Label("\(entry.openIssuesCount)", systemImage: "exclamationmark.bubble")
                        .font(.caption2)
                        .foregroundStyle(.orange)
                }
            }
        }
        .padding()
        .containerBackground(for: .widget) {
            Color(.windowBackgroundColor)
        }
    }
}

// ─── Medium: next 3 events + 3 tasks ─────────────────────────────────────────

private struct MediumWidgetView: View {
    let entry: DashboardWidgetEntry

    var body: some View {
        HStack(alignment: .top, spacing: 12) {
            VStack(alignment: .leading, spacing: 4) {
                Label("Events", systemImage: "calendar")
                    .font(.caption2.weight(.semibold))
                    .foregroundStyle(.secondary)
                ForEach(entry.upcomingEvents.prefix(3)) { event in
                    HStack(spacing: 4) {
                        Circle()
                            .fill(Color.accentColor)
                            .frame(width: 6, height: 6)
                        Text(event.summary)
                            .font(.caption)
                            .lineLimit(1)
                    }
                    Text(event.start.formatted(date: .omitted, time: .shortened))
                        .font(.caption2)
                        .foregroundStyle(.secondary)
                        .padding(.leading, 10)
                }
                if entry.upcomingEvents.isEmpty {
                    Text("None upcoming")
                        .font(.caption2)
                        .foregroundStyle(.tertiary)
                }
                Spacer()
            }
            .frame(maxWidth: .infinity, alignment: .leading)

            Divider()

            VStack(alignment: .leading, spacing: 4) {
                Label("Tasks", systemImage: "checklist")
                    .font(.caption2.weight(.semibold))
                    .foregroundStyle(.secondary)
                ForEach(entry.dueSoonTasks.prefix(3)) { task in
                    HStack(spacing: 4) {
                        Circle()
                            .fill(task.isOverdue ? Color.red : Color.accentColor)
                            .frame(width: 6, height: 6)
                        Text(task.summary)
                            .font(.caption)
                            .lineLimit(1)
                            .foregroundStyle(task.isOverdue ? .red : .primary)
                    }
                }
                if entry.dueSoonTasks.isEmpty {
                    Text("All caught up!")
                        .font(.caption2)
                        .foregroundStyle(.tertiary)
                }
                Spacer()
            }
            .frame(maxWidth: .infinity, alignment: .leading)
        }
        .padding()
        .containerBackground(for: .widget) {
            Color(.windowBackgroundColor)
        }
    }
}

// ─── Large: events + tasks + issues count ─────────────────────────────────────

private struct LargeWidgetView: View {
    let entry: DashboardWidgetEntry

    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            Label("CrossDashboard", systemImage: "square.grid.2x2.fill")
                .font(.caption.weight(.semibold))
                .foregroundStyle(.secondary)

            Divider()

            SectionHeader(title: "Upcoming Events", icon: "calendar")
            ForEach(entry.upcomingEvents.prefix(3)) { event in
                HStack {
                    Circle().fill(Color.accentColor).frame(width: 7, height: 7)
                    Text(event.summary).font(.caption).lineLimit(1)
                    Spacer()
                    Text(event.start, style: .time).font(.caption2).foregroundStyle(.secondary)
                }
            }
            if entry.upcomingEvents.isEmpty {
                Text("No upcoming events").font(.caption2).foregroundStyle(.tertiary)
            }

            Divider()

            SectionHeader(title: "Due Soon", icon: "checklist")
            ForEach(entry.dueSoonTasks.prefix(3)) { task in
                HStack {
                    Circle()
                        .fill(task.isOverdue ? Color.red : Color.accentColor)
                        .frame(width: 7, height: 7)
                    Text(task.summary)
                        .font(.caption)
                        .lineLimit(1)
                        .foregroundStyle(task.isOverdue ? .red : .primary)
                    Spacer()
                    if let due = task.due {
                        Text(due, style: .relative)
                            .font(.caption2)
                            .foregroundStyle(task.isOverdue ? .red : .secondary)
                    }
                }
            }
            if entry.dueSoonTasks.isEmpty {
                Text("All caught up!").font(.caption2).foregroundStyle(.tertiary)
            }

            Divider()

            HStack {
                Image(systemName: "exclamationmark.bubble")
                    .foregroundStyle(.orange)
                Text("\(entry.openIssuesCount) open issue\(entry.openIssuesCount == 1 ? "" : "s")")
                    .font(.caption)
            }

            Spacer()
        }
        .padding()
        .containerBackground(for: .widget) {
            Color(.windowBackgroundColor)
        }
    }
}

private struct SectionHeader: View {
    let title: String
    let icon: String
    var body: some View {
        Label(title, systemImage: icon)
            .font(.caption2.weight(.semibold))
            .foregroundStyle(.secondary)
    }
}

// ─── Widget definition ────────────────────────────────────────────────────────

struct DashboardWidget: Widget {
    let kind = "DashboardWidget"

    var body: some WidgetConfiguration {
        StaticConfiguration(kind: kind, provider: DashboardWidgetProvider()) { entry in
            DashboardWidgetEntryView(entry: entry)
        }
        .configurationDisplayName("CrossDashboard")
        .description("Your upcoming events, tasks due soon, and open issues at a glance.")
        .supportedFamilies([.systemSmall, .systemMedium, .systemLarge])
    }
}
