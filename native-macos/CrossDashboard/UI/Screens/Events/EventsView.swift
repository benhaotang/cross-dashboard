import SwiftUI
import CrossDashboardKit

/// Calendar event list with day/week/month filter and search.
/// Mirrors EventsScreen on Android.
struct EventsView: View {

    @Environment(AppViewModel.self) private var appViewModel
    @State private var viewModel = EventsViewModel()

    var body: some View {
        @Bindable var vm = viewModel
        Group {
            if viewModel.isLoading && viewModel.allEvents.isEmpty {
                ProgressView("Loading events…")
                    .frame(maxWidth: .infinity, maxHeight: .infinity)
            } else if viewModel.filteredEvents.isEmpty {
                ContentUnavailableView(
                    searchEmptyTitle,
                    systemImage: "calendar",
                    description: Text(searchEmptyDescription)
                )
            } else {
                List(viewModel.filteredEvents, selection: $vm.selectedEventID) { event in
                    EventRow(event: event)
                        .tag(event.uid)
                        .contextMenu {
                            Button(role: .destructive) {
                                viewModel.delete(event)
                            } label: {
                                Label("Delete", systemImage: "trash")
                            }
                        }
                        .keyboardShortcut(.delete, modifiers: .command)
                }
                .listStyle(.inset)
            }
        }
        .navigationTitle("Events")
        .searchable(text: $vm.searchText, prompt: "Search events")
        .toolbar {
            ToolbarItem(placement: .primaryAction) {
                Picker("Period", selection: $vm.filter) {
                    ForEach(EventsViewModel.Filter.allCases) { f in
                        Text(f.rawValue).tag(f)
                    }
                }
                .pickerStyle(.segmented)
                .accessibilityLabel("Filter by time period")
            }
            ToolbarItem {
                Button {
                    Task { await viewModel.sync() }
                } label: {
                    Label("Sync", systemImage: "arrow.clockwise")
                }
                .accessibilityLabel("Sync events")
            }
        }
        .onChange(of: viewModel.selectedEventID) { _, id in
            appViewModel.selectedEventID = id
        }
        .onAppear {
            viewModel.selectedEventID = appViewModel.selectedEventID
        }
        .onChange(of: appViewModel.selectedEventID) { _, id in
            viewModel.selectedEventID = id
        }
        .alert("Error", isPresented: Binding(
            get: { viewModel.errorMessage != nil },
            set: { if !$0 { viewModel.errorMessage = nil } }
        )) {
            Button("OK") { viewModel.errorMessage = nil }
        } message: {
            Text(viewModel.errorMessage ?? "")
        }
    }

    private var searchEmptyTitle: String {
        viewModel.searchText.isEmpty ? "No events" : "No results"
    }

    private var searchEmptyDescription: String {
        viewModel.searchText.isEmpty
            ? "No events in the selected time period."
            : "No events match '\(viewModel.searchText)'."
    }
}

// ─── EventRow ─────────────────────────────────────────────────────────────────

private struct EventRow: View {
    let event: CalendarEvent

    var body: some View {
        HStack(spacing: 10) {
            CalendarColorDot(calendarHref: event.calendarHref)
                .accessibilityHidden(true)

            VStack(alignment: .leading, spacing: 2) {
                Text(event.summary)
                    .fontWeight(.medium)
                    .lineLimit(1)

                HStack(spacing: 6) {
                    Text(event.start, style: .date)
                        .font(.caption)
                        .foregroundStyle(.secondary)
                    Text(event.start, style: .time)
                        .font(.caption)
                        .foregroundStyle(.secondary)
                    if let loc = event.location {
                        Text("· \(loc)")
                            .font(.caption)
                            .foregroundStyle(.secondary)
                            .lineLimit(1)
                    }
                }
            }

            Spacer()

            Text(durationLabel)
                .font(.caption2)
                .foregroundStyle(.tertiary)
        }
        .padding(.vertical, 4)
        .accessibilityElement(children: .combine)
        .accessibilityLabel(event.summary)
        .accessibilityValue("\(event.start.formatted(date: .abbreviated, time: .shortened)), \(durationLabel)")
        .accessibilityHint(event.location.map { "at \($0)" } ?? "Activate to view details")
    }

    private var durationLabel: String {
        let minutes = Int(event.end.timeIntervalSince(event.start) / 60)
        if minutes < 60 { return "\(minutes)m" }
        let h = minutes / 60; let m = minutes % 60
        return m == 0 ? "\(h)h" : "\(h)h \(m)m"
    }
}
