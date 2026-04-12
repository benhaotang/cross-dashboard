import SwiftUI
import CrossDashboardKit

/// Read-only event detail panel.
/// Displayed in the NavigationSplitView detail column.
struct EventDetailView: View {

    @Environment(\.appContainer) private var container
    let eventID: String?

    private var event: CalendarEvent? {
        guard let id = eventID else { return nil }
        return container.eventRepository.events.first { $0.uid == id }
    }

    var body: some View {
        if let event {
            PropertyDetailShell(title: event.summary) {
                eventContent(event)
            }
        } else {
            ContentUnavailableView(
                "Select an event",
                systemImage: "calendar",
                description: Text("Choose an event from the list to see its details.")
            )
        }
    }

    @ViewBuilder
    private func eventContent(_ event: CalendarEvent) -> some View {
        // Calendar color + name
        if let href = event.calendarHref {
            let calName = container.preferences.cachedCalendars.first { $0.href == href }?.displayName ?? href
            HStack(spacing: 8) {
                CalendarColorDot(calendarHref: href)
                Text(calName)
                    .foregroundStyle(.secondary)
            }
            .accessibilityElement(children: .combine)
            .accessibilityLabel("Calendar: \(calName)")
        }

        Divider()

        // Date / time
        ReadField(label: "Starts") {
            Text(event.start, style: .date) + Text("  ") + Text(event.start, style: .time)
        }

        ReadField(label: "Ends") {
            Text(event.end, style: .date) + Text("  ") + Text(event.end, style: .time)
        }

        let minutes = Int(event.end.timeIntervalSince(event.start) / 60)
        ReadField(label: "Duration", text: formatDuration(minutes))

        // Location
        if let location = event.location, !location.isEmpty {
            ReadField(label: "Location", text: location)
        }

        // Description
        if let desc = event.description, !desc.isEmpty {
            Divider()
            ReadMarkdownField(label: "Description", content: desc)
        }
    }

    private func formatDuration(_ minutes: Int) -> String {
        if minutes < 60 { return "\(minutes) min" }
        let h = minutes / 60; let m = minutes % 60
        return m == 0 ? "\(h) hr" : "\(h) hr \(m) min"
    }
}
