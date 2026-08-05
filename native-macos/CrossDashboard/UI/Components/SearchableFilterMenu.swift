import SwiftUI

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
        if labels.count == 1 { return labels[0] }
        return "\(title) (\(labels.count))"
    }

    var body: some View {
        Button {
            isPresented = true
        } label: {
            Label(buttonLabel, systemImage: "chevron.down")
        }
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
            .frame(width: 300, height: min(420, CGFloat(max(3, filtered.count)) * 32 + (searchable ? 100 : 64)))
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
