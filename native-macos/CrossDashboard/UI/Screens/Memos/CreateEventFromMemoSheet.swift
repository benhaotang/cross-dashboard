import SwiftUI
import CrossDashboardKit

struct CreateEventFromMemoSheetMac: View {

    let memo: MemosMemo
    var viewModel: MemosViewModel
    @Environment(\.dismiss) private var dismiss
    @Environment(\.appContainer) private var container

    @State private var summary: String = ""
    @State private var description: String = ""
    @State private var startDate: Date = Date()
    @State private var endDate: Date = Date().addingTimeInterval(3600)
    @State private var calendarHref: String = ""
    @State private var calendars: [(String, String)] = []

    var body: some View {
        Form {
            Section("Event Details") {
                TextField("Summary", text: $summary)
                TextEditor(text: $description)
                    .frame(minHeight: 60)
                DatePicker("Start", selection: $startDate)
                DatePicker("End",   selection: $endDate)
            }
            if !calendars.isEmpty {
                Section("Calendar") {
                    Picker("Calendar", selection: $calendarHref) {
                        ForEach(calendars, id: \.0) { (href, name) in
                            Text(name).tag(href)
                        }
                    }
                }
            }
            Section {
                HStack {
                    Spacer()
                    Button("Cancel", role: .cancel) { dismiss() }
                    Button("Create Event") {
                        Task {
                            await viewModel.createEventFromMemo(
                                summary: summary,
                                description: description,
                                start: startDate,
                                end: endDate,
                                calendarHref: calendarHref
                            )
                            dismiss()
                        }
                    }
                    .buttonStyle(.borderedProminent)
                    .disabled(summary.isEmpty || calendarHref.isEmpty)
                }
            }
        }
        .formStyle(.grouped)
        .padding()
        .frame(minWidth: 460, minHeight: 360)
        .onAppear {
            let host = viewModel.memosHost
            let memoUrl = host.isEmpty ? "" : "\(host)/\(memo.name)"
            summary = memo.property.title.isEmpty
                ? (memo.content.components(separatedBy: "\n").first(where: { !$0.isEmpty })?.prefix(80).description ?? "")
                : memo.property.title
            description = memoUrl.isEmpty ? memo.snippet : "\(memoUrl)\n\n\(memo.snippet)"
            if let seed = viewModel.detectFirstDate(in: memo) { startDate = seed }
            endDate = startDate.addingTimeInterval(3600)
            if let raw = container.keychain.get(CredentialKey.caldavSelectedCalendars),
               let cals = try? JSONDecoder().decode([CalDavCalendar].self, from: Data(raw.utf8)) {
                calendars = cals.map { ($0.href, $0.displayName) }
                calendarHref = container.keychain.get(CredentialKey.caldavDefaultEventCalendar) ?? cals.first?.href ?? ""
            }
        }
    }
}
