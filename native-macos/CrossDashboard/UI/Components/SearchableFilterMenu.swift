import SwiftUI
import AppKit

struct FilterMenuOption: Identifiable, Hashable {
    let id: String
    let label: String
}

struct SearchableFilterMenu: View {
    let title: String
    let options: [FilterMenuOption]
    let selected: Set<String>
    var allowsMultiple = false
    var searchable = false
    var defaultSelected: Set<String>? = nil
    let onChange: (Set<String>) -> Void

    @State private var isPresented = false
    @State private var query = ""

    private var filtered: [FilterMenuOption] {
        guard !query.isEmpty else { return options }
        return options.filter { $0.label.localizedCaseInsensitiveContains(query) }
    }

    private var buttonLabel: String {
        let labels = options.filter { selected.contains($0.id) }.map(\.label)
        if labels.isEmpty { return title }
        if labels.count == 1 { return "\(title) – \(labels[0])" }
        return "\(title) – \(labels.count) selected"
    }

    private var inferredDefault: Set<String> {
        if let defaultSelected { return defaultSelected }
        if allowsMultiple { return [] }
        return options.first.map { [$0.id] } ?? []
    }

    private var iconName: String {
        switch title {
        case "Tags": "tag"
        case "Time range": "calendar"
        case "Type": "square.grid.2x2"
        case "Status": "checkmark.circle"
        case "Milestone": "flag"
        default: "line.3.horizontal.decrease.circle"
        }
    }

    private func optionIcon(_ option: FilterMenuOption) -> String? {
        let value = option.id.lowercased()
        switch title {
        case "Status":
            switch value {
            case "open": return "circle"
            case "closed", "completed": return "checkmark.circle.fill"
            case "archived": return "archivebox"
            case "active", "normal": return "bolt.circle"
            case "all": return "list.bullet"
            default: return nil
            }
        case "Type":
            switch value {
            case "events": return "calendar"
            case "tasks": return "checkmark.square"
            case "issues": return "exclamationmark.bubble"
            case "all": return "square.grid.2x2"
            default: return nil
            }
        case "Time range":
            switch value {
            case "today": return "calendar.circle"
            case "tomorrow": return "calendar.badge.clock"
            case "this week", "week": return "calendar"
            case "all": return "infinity"
            default: return nil
            }
        case "Milestone": return "flag"
        default: return nil
        }
    }

    var body: some View {
        Button {
            isPresented = true
        } label: {
            Label(buttonLabel, systemImage: iconName)
        }
        .buttonStyle(.bordered)
        .tint(selected == inferredDefault ? Color(nsColor: .secondaryLabelColor) : Color.accentColor)
        .popover(isPresented: $isPresented, arrowEdge: .bottom) {
            VStack(alignment: .leading, spacing: 8) {
                Text(title).font(.headline)
                if searchable {
                    TextField("Search \(title.lowercased())", text: $query)
                        .textFieldStyle(.roundedBorder)
                }
                ScrollView {
                    LazyVStack(spacing: 2) {
                        ForEach(filtered) { option in
                            Button {
                                if allowsMultiple {
                                    onChange(selected.contains(option.id)
                                        ? selected.subtracting([option.id])
                                        : selected.union([option.id]))
                                } else {
                                    onChange([option.id])
                                    isPresented = false
                                }
                            } label: {
                                HStack {
                                    Image(systemName: selected.contains(option.id)
                                        ? (allowsMultiple ? "checkmark.square.fill" : "largecircle.fill.circle")
                                        : (allowsMultiple ? "square" : "circle"))
                                    if let optionIcon = optionIcon(option) {
                                        Image(systemName: optionIcon)
                                            .foregroundStyle(.secondary)
                                            .frame(width: 18)
                                    }
                                    Text(option.label)
                                    Spacer()
                                }
                                .contentShape(Rectangle())
                                .padding(.vertical, 5)
                            }
                            .buttonStyle(.plain)
                        }
                    }
                }
            }
            .padding(12)
            .frame(width: 360, height: min(520, CGFloat(max(5, filtered.count)) * 36 + (searchable ? 116 : 76)))
        }
        .onChange(of: isPresented) { _, presented in
            if !presented { query = "" }
        }
    }
}

struct ClearFiltersButton: View {
    let action: () -> Void

    var body: some View {
        Button(action: action) {
            Image(systemName: "xmark")
        }
        .help("Clear filters")
        .accessibilityLabel("Clear filters")
    }
}
