import WidgetKit
import SwiftUI
import CrossDashboardKit

// ─── Entry ────────────────────────────────────────────────────────────────────

struct DashboardWidgetEntry: TimelineEntry {
    let date: Date
    let upcomingEvents: [WidgetUpcomingEvent]
    let dueSoonTasks: [WidgetDueTask]
    let openIssuesCount: Int
    /// True when no snapshot has been written yet (app never synced).
    let isEmpty: Bool
}

// ─── Timeline Provider ────────────────────────────────────────────────────────
// Data flows via App Group UserDefaults (written by AppContainer.writeWidgetSnapshot()
// after every sync). This is simpler and more reliable than sharing a live
// SwiftData store across process boundaries.

struct DashboardWidgetProvider: TimelineProvider {

    func placeholder(in context: Context) -> DashboardWidgetEntry {
        .placeholder
    }

    func getSnapshot(in context: Context, completion: @escaping (DashboardWidgetEntry) -> Void) {
        completion(context.isPreview ? .placeholder : loadEntry())
    }

    func getTimeline(in context: Context, completion: @escaping (Timeline<DashboardWidgetEntry>) -> Void) {
        let entry = loadEntry()
        // Use the same sync interval the user configured. The main app also calls
        // WidgetCenter.shared.reloadAllTimelines() after each sync, so this is just
        // a safety-net fallback in case the app hasn't run.
        let intervalMinutes = WidgetDataStore.load()?.syncIntervalMinutes ?? 60
        let nextRefresh = Calendar.current.date(byAdding: .minute, value: intervalMinutes, to: Date()) ?? Date()
        completion(Timeline(entries: [entry], policy: .after(nextRefresh)))
    }

    // ─── Data loading ─────────────────────────────────────────────────────────

    private func loadEntry() -> DashboardWidgetEntry {
        guard let snapshot = WidgetDataStore.load() else {
            return DashboardWidgetEntry(date: Date(), upcomingEvents: [], dueSoonTasks: [], openIssuesCount: 0, isEmpty: true)
        }
        return DashboardWidgetEntry(
            date: Date(timeIntervalSince1970: snapshot.writtenAt),
            upcomingEvents: snapshot.upcomingEvents,
            dueSoonTasks: snapshot.dueSoonTasks,
            openIssuesCount: snapshot.openIssuesCount,
            isEmpty: false
        )
    }
}

// ─── Entry placeholder ────────────────────────────────────────────────────────

extension DashboardWidgetEntry {
    static let placeholder = DashboardWidgetEntry(
        date: Date(),
        upcomingEvents: [
            WidgetUpcomingEvent(id: "1", summary: "Team standup",    startEpoch: Date().addingTimeInterval(3600).timeIntervalSince1970, calendarColor: nil),
            WidgetUpcomingEvent(id: "2", summary: "Design review",   startEpoch: Date().addingTimeInterval(7200).timeIntervalSince1970, calendarColor: nil),
        ],
        dueSoonTasks: [
            WidgetDueTask(id: "a", summary: "Write release notes", dueEpoch: Date().addingTimeInterval(86400).timeIntervalSince1970, isOverdue: false, priority: 1),
            WidgetDueTask(id: "b", summary: "Fix CI flake",        dueEpoch: Date().addingTimeInterval(-3600).timeIntervalSince1970,  isOverdue: true,  priority: 5),
        ],
        openIssuesCount: 4,
        isEmpty: false
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

            if entry.isEmpty {
                Text("Open the app to sync")
                    .font(.caption2)
                    .foregroundStyle(.secondary)
            } else if let next = entry.upcomingEvents.first {
                VStack(alignment: .leading, spacing: 2) {
                    Text(next.summary)
                        .font(.caption.weight(.medium))
                        .lineLimit(2)
                    Text(Date(timeIntervalSince1970: next.startEpoch), style: .relative)
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
                ForEach(Array(entry.upcomingEvents.prefix(3))) { event in
                    HStack(spacing: 4) {
                        Circle()
                            .fill(Color.accentColor)
                            .frame(width: 6, height: 6)
                        Text(event.summary)
                            .font(.caption)
                            .lineLimit(1)
                    }
                    Text(Date(timeIntervalSince1970: event.startEpoch).formatted(date: .omitted, time: .shortened))
                        .font(.caption2)
                        .foregroundStyle(.secondary)
                        .padding(.leading, 10)
                }
                if entry.upcomingEvents.isEmpty {
                    Text(entry.isEmpty ? "Sync to load" : "None upcoming")
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
                ForEach(Array(entry.dueSoonTasks.prefix(3))) { task in
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
                    Text(entry.isEmpty ? "Sync to load" : "All caught up!")
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
            ForEach(Array(entry.upcomingEvents.prefix(3))) { event in
                HStack {
                    Circle().fill(Color.accentColor).frame(width: 7, height: 7)
                    Text(event.summary).font(.caption).lineLimit(1)
                    Spacer()
                    Text(Date(timeIntervalSince1970: event.startEpoch), style: .time)
                        .font(.caption2).foregroundStyle(.secondary)
                }
            }
            if entry.upcomingEvents.isEmpty {
                Text(entry.isEmpty ? "Open app and sync" : "No upcoming events")
                    .font(.caption2).foregroundStyle(.tertiary)
            }

            Divider()

            SectionHeader(title: "Due Soon", icon: "checklist")
            ForEach(Array(entry.dueSoonTasks.prefix(3))) { task in
                HStack {
                    Circle()
                        .fill(task.isOverdue ? Color.red : Color.accentColor)
                        .frame(width: 7, height: 7)
                    Text(task.summary)
                        .font(.caption)
                        .lineLimit(1)
                        .foregroundStyle(task.isOverdue ? .red : .primary)
                    Spacer()
                    if let dueEpoch = task.dueEpoch {
                        Text(Date(timeIntervalSince1970: dueEpoch), style: .relative)
                            .font(.caption2)
                            .foregroundStyle(task.isOverdue ? .red : .secondary)
                    }
                }
            }
            if entry.dueSoonTasks.isEmpty {
                Text(entry.isEmpty ? "Open app and sync" : "All caught up!")
                    .font(.caption2).foregroundStyle(.tertiary)
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
