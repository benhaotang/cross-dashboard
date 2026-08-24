import WidgetKit
import SwiftUI
import CrossDashboardKit
import OSLog

private let widgetTimelineLogger = Logger(
    subsystem: Bundle.main.bundleIdentifier ?? "com.crossdashboard.app.widget",
    category: "WidgetTimeline"
)

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
        let entry = context.isPreview ? .placeholder : loadEntry(from: WidgetDataStore.load())
        widgetTimelineLogger.info(
            "Snapshot completed, preview=\(context.isPreview), empty=\(entry.isEmpty), events=\(entry.upcomingEvents.count), tasks=\(entry.dueSoonTasks.count), issues=\(entry.openIssuesCount)"
        )
        completion(entry)
    }

    func getTimeline(in context: Context, completion: @escaping (Timeline<DashboardWidgetEntry>) -> Void) {
        let snapshot = WidgetDataStore.load()
        let entry = loadEntry(from: snapshot)
        // Use the same sync interval the user configured. The main app also calls
        // WidgetCenter.shared.reloadAllTimelines() after each sync, so this is just
        // a safety-net fallback in case the app hasn't run.
        let intervalMinutes = snapshot?.syncIntervalMinutes ?? 60
        let nextRefresh = Calendar.current.date(byAdding: .minute, value: intervalMinutes, to: Date()) ?? Date()
        widgetTimelineLogger.info(
            "Timeline completed, family=\(String(describing: context.family), privacy: .public), empty=\(entry.isEmpty), events=\(entry.upcomingEvents.count), tasks=\(entry.dueSoonTasks.count), issues=\(entry.openIssuesCount), refreshMinutes=\(intervalMinutes)"
        )
        completion(Timeline(entries: [entry], policy: .after(nextRefresh)))
    }

    // ─── Data loading ─────────────────────────────────────────────────────────

    private func loadEntry(from snapshot: WidgetSnapshot?) -> DashboardWidgetEntry {
        guard let snapshot else {
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

// ─── Small: next event + workload counts ─────────────────────────────────────

private struct SmallWidgetView: View {
    let entry: DashboardWidgetEntry

    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            Label("Next event", systemImage: "calendar")
                .font(.caption2.weight(.semibold))
                .foregroundStyle(.secondary)

            if entry.isEmpty {
                Text("Open the app to sync")
                    .font(.caption2)
                    .foregroundStyle(.secondary)
                    .frame(maxHeight: .infinity, alignment: .center)
            } else if let next = entry.upcomingEvents.first {
                VStack(alignment: .leading, spacing: 4) {
                    Text(next.summary)
                        .font(.caption.weight(.semibold))
                        .lineLimit(2)
                    Text(Date(timeIntervalSince1970: next.startEpoch), style: .relative)
                        .font(.caption2)
                        .foregroundStyle(.secondary)
                }
                .frame(maxHeight: .infinity, alignment: .center)
            } else {
                Text("Nothing upcoming")
                    .font(.caption.weight(.medium))
                    .foregroundStyle(.secondary)
                    .frame(maxHeight: .infinity, alignment: .center)
            }

            Divider()

            HStack(spacing: 10) {
                Label("\(entry.dueSoonTasks.count) due", systemImage: "checklist")
                    .font(.caption2)
                    .foregroundStyle(.primary)
                Spacer(minLength: 0)
                Label("\(entry.openIssuesCount) open", systemImage: "exclamationmark.bubble")
                    .font(.caption2)
                    .foregroundStyle(entry.openIssuesCount > 0 ? Color.orange : Color.secondary)
            }
        }
        .padding()
        .containerBackground(for: .widget) {
            Color(.windowBackgroundColor)
        }
    }
}

// ─── Medium: paired lists + issue footer ─────────────────────────────────────

private struct MediumWidgetView: View {
    let entry: DashboardWidgetEntry

    var body: some View {
        VStack(spacing: 8) {
            HStack(alignment: .top, spacing: 12) {
                VStack(alignment: .leading, spacing: 5) {
                    SectionHeader(title: "Events", icon: "calendar")
                    ForEach(Array(entry.upcomingEvents.prefix(2))) { event in
                        EventWidgetRow(event: event, showsDate: false)
                    }
                    if entry.upcomingEvents.isEmpty {
                        Text(entry.isEmpty ? "Sync to load" : "None upcoming")
                            .font(.caption2)
                            .foregroundStyle(.tertiary)
                    }
                    Spacer(minLength: 0)
                }
                .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .topLeading)

                Divider()

                VStack(alignment: .leading, spacing: 5) {
                    SectionHeader(title: "Due soon", icon: "checklist")
                    ForEach(Array(entry.dueSoonTasks.prefix(2))) { task in
                        TaskWidgetRow(task: task, showsDueDate: false)
                    }
                    if entry.dueSoonTasks.isEmpty {
                        Text(entry.isEmpty ? "Sync to load" : "All caught up")
                            .font(.caption2)
                            .foregroundStyle(.tertiary)
                    }
                    Spacer(minLength: 0)
                }
                .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .topLeading)
            }

            Divider()

            IssueSummary(openIssuesCount: entry.openIssuesCount)
        }
        .padding()
        .containerBackground(for: .widget) {
            Color(.windowBackgroundColor)
        }
    }
}

// ─── Large: two-column day board ─────────────────────────────────────────────

private struct LargeWidgetView: View {
    let entry: DashboardWidgetEntry

    var body: some View {
        VStack(spacing: 10) {
            HStack(alignment: .top, spacing: 14) {
                VStack(alignment: .leading, spacing: 8) {
                    SectionHeader(title: "Upcoming", icon: "calendar")
                    ForEach(Array(entry.upcomingEvents.prefix(5))) { event in
                        EventWidgetRow(event: event, showsDate: true)
                    }
                    if entry.upcomingEvents.isEmpty {
                        EmptySectionMessage(
                            text: entry.isEmpty ? "Open the app to sync" : "No upcoming events"
                        )
                    }
                    Spacer(minLength: 0)
                }
                .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .topLeading)

                Divider()

                VStack(alignment: .leading, spacing: 8) {
                    SectionHeader(title: "Due soon", icon: "checklist")
                    ForEach(Array(entry.dueSoonTasks.prefix(5))) { task in
                        TaskWidgetRow(task: task, showsDueDate: true)
                    }
                    if entry.dueSoonTasks.isEmpty {
                        EmptySectionMessage(
                            text: entry.isEmpty ? "Open the app to sync" : "All caught up"
                        )
                    }
                    Spacer(minLength: 0)
                }
                .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .topLeading)
            }

            Divider()

            IssueSummary(openIssuesCount: entry.openIssuesCount)
        }
        .padding()
        .containerBackground(for: .widget) {
            Color(.windowBackgroundColor)
        }
    }
}

private struct EventWidgetRow: View {
    let event: WidgetUpcomingEvent
    let showsDate: Bool

    var body: some View {
        HStack(alignment: .top, spacing: 7) {
            RoundedRectangle(cornerRadius: 1)
                .fill(Color.accentColor)
                .frame(width: 3, height: showsDate ? 30 : 18)

            VStack(alignment: .leading, spacing: 2) {
                Text(event.summary)
                    .font(.caption.weight(.medium))
                    .lineLimit(showsDate ? 2 : 1)
                Text(eventDate.formatted(
                    showsDate
                        ? .dateTime.weekday(.abbreviated).hour().minute()
                        : .dateTime.hour().minute()
                ))
                    .font(.caption2)
                    .foregroundStyle(.secondary)
            }
        }
    }

    private var eventDate: Date {
        Date(timeIntervalSince1970: event.startEpoch)
    }
}

private struct TaskWidgetRow: View {
    let task: WidgetDueTask
    let showsDueDate: Bool

    var body: some View {
        HStack(alignment: .top, spacing: 7) {
            Circle()
                .strokeBorder(task.isOverdue ? Color.red : Color.accentColor, lineWidth: 1.5)
                .frame(width: 8, height: 8)
                .padding(.top, 3)

            VStack(alignment: .leading, spacing: 2) {
                Text(task.summary)
                    .font(.caption.weight(.medium))
                    .lineLimit(showsDueDate ? 2 : 1)
                    .foregroundStyle(task.isOverdue ? Color.red : Color.primary)
                if showsDueDate, let dueEpoch = task.dueEpoch {
                    Text(Date(timeIntervalSince1970: dueEpoch), style: .relative)
                        .font(.caption2)
                        .foregroundStyle(task.isOverdue ? Color.red : Color.secondary)
                }
            }
        }
    }
}

private struct EmptySectionMessage: View {
    let text: String

    var body: some View {
        Text(text)
            .font(.caption)
            .foregroundStyle(.tertiary)
            .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .center)
            .multilineTextAlignment(.center)
    }
}

private struct IssueSummary: View {
    let openIssuesCount: Int

    var body: some View {
        HStack(spacing: 6) {
            Image(systemName: "exclamationmark.bubble")
            Text("\(openIssuesCount)")
                .font(.caption.weight(.semibold))
            Text(openIssuesCount == 1 ? "open issue" : "open issues")
                .font(.caption2)
            Spacer(minLength: 0)
        }
        .foregroundStyle(openIssuesCount > 0 ? Color.orange : Color.secondary)
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
