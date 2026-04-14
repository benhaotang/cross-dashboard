import SwiftUI
import CrossDashboardKit

struct ExtractTasksSheetMac: View {

    let memo: MemosMemo
    var viewModel: MemosViewModel
    @Environment(\.dismiss) private var dismiss
    @Environment(\.appContainer) private var container

    @State private var tasks: [ParsedTask] = []
    @State private var selected: Set<Int> = []
    @State private var calendarHref: String = ""
    @State private var calendars: [(String, String)] = []

    var body: some View {
        VStack(alignment: .leading, spacing: 16) {
            Text("Extract Tasks").font(.title2).fontWeight(.semibold)

            if tasks.isEmpty {
                Text("No incomplete tasks (- [ ] …) found in this memo.")
                    .foregroundStyle(.secondary)
            } else {
                Text("Select tasks to create:").font(.caption).foregroundStyle(.secondary)
                List(Array(tasks.enumerated()), id: \.offset, selection: $selected) { idx, task in
                    VStack(alignment: .leading, spacing: 2) {
                        Text(task.summary).font(.body)
                        let meta = [
                            task.priority > 0 ? "P\(task.priority)" : nil,
                            task.due.map { DateFormatter.localizedString(from: $0, dateStyle: .short, timeStyle: .none) },
                            task.categories.isEmpty ? nil : task.categories.map { "#\($0)" }.joined(separator: " ")
                        ].compactMap { $0 }.joined(separator: " · ")
                        if !meta.isEmpty {
                            Text(meta).font(.caption).foregroundStyle(.secondary)
                        }
                    }
                    .tag(idx)
                }
                .frame(height: min(CGFloat(tasks.count) * 56, 280))
            }

            if !calendars.isEmpty {
                Picker("Calendar", selection: $calendarHref) {
                    ForEach(calendars, id: \.0) { (href, name) in
                        Text(name).tag(href)
                    }
                }
            }

            HStack {
                Spacer()
                Button("Cancel", role: .cancel) { dismiss() }
                Button("Create \(selected.count) Task\(selected.count != 1 ? "s" : "")") {
                    Task {
                        for idx in selected {
                            await viewModel.createTaskFromParsed(tasks[idx], calendarHref: calendarHref)
                        }
                        dismiss()
                    }
                }
                .buttonStyle(.borderedProminent)
                .disabled(selected.isEmpty || calendarHref.isEmpty)
            }
        }
        .padding(24)
        .frame(minWidth: 460, minHeight: 300)
        .onAppear {
            tasks = viewModel.extractTasks(from: memo)
            selected = Set(tasks.indices)
            if let raw = container.keychain.get(CredentialKey.caldavSelectedCalendars),
               let cals = try? JSONDecoder().decode([CalDavCalendar].self, from: Data(raw.utf8)) {
                calendars = cals.map { ($0.href, $0.displayName) }
                calendarHref = container.keychain.get(CredentialKey.caldavDefaultTaskCalendar) ?? cals.first?.href ?? ""
            }
        }
    }
}
