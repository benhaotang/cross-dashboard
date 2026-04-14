import SwiftUI
import CrossDashboardKit
import UniformTypeIdentifiers

/// List column for the Memos screen — mirrors MemosScreen (Android list pane).
struct MemosView: View {

    @Environment(AppViewModel.self) private var appViewModel
    @State private var viewModel = MemosViewModel()
    @State private var showCreateSheet = false
    @State private var captureInitialText: String = ""

    var body: some View {
        VStack(spacing: 0) {
            // ── Filter toolbar ────────────────────────────────────────────────
            ScrollView(.horizontal, showsIndicators: false) {
                HStack(spacing: 8) {
                    filterChip("Normal", isSelected: viewModel.stateFilter == .normal) {
                        viewModel.stateFilter = viewModel.stateFilter == .normal ? nil : .normal
                    }
                    filterChip("Archived", isSelected: viewModel.stateFilter == .archived) {
                        viewModel.stateFilter = viewModel.stateFilter == .archived ? nil : .archived
                    }
                    Divider().frame(height: 20)
                    ForEach(viewModel.allTags, id: \.self) { tag in
                        filterChip("#\(tag)", isSelected: viewModel.selectedTag == tag) {
                            viewModel.selectedTag = viewModel.selectedTag == tag ? nil : tag
                        }
                    }
                }
                .padding(.horizontal, 12)
                .padding(.vertical, 6)
            }
            Divider()

            if viewModel.isLoading {
                ProgressView().padding()
            }

            // ── Empty state ───────────────────────────────────────────────────
            if !viewModel.isLoading && viewModel.filteredMemos.isEmpty {
                ContentUnavailableView {
                    Label("No Memos", systemImage: "tray.and.arrow.down")
                } description: {
                    Text("Create a memo with the + button, or sync to load from your Memos server.")
                } actions: {
                    Button("Sync Now") { Task { await viewModel.sync() } }
                        .buttonStyle(.borderedProminent)
                }
                .frame(maxWidth: .infinity, maxHeight: .infinity)
            }

            // ── Memo list ─────────────────────────────────────────────────────
            List(viewModel.filteredMemos, selection: Binding(
                get: { appViewModel.selectedMemoID },
                set: { appViewModel.selectedMemoID = $0 }
            )) { memo in
                MemoListRow(memo: memo)
                    .tag(memo.name)
                    .swipeActions(edge: .trailing, allowsFullSwipe: true) {
                        Button(role: .destructive) {
                            Task { await viewModel.deleteMemo(memo.name) }
                        } label: { Label("Delete", systemImage: "trash") }
                    }
                    .swipeActions(edge: .leading) {
                        Button {
                            Task {
                                if memo.state == .archived {
                                    await viewModel.restoreMemo(memo.name)
                                } else {
                                    await viewModel.archiveMemo(memo.name)
                                }
                            }
                        } label: {
                            Label(memo.state == .archived ? "Restore" : "Archive",
                                  systemImage: memo.state == .archived ? "tray.and.arrow.up" : "archivebox")
                        }
                        .tint(.orange)
                    }
                    .contextMenu {
                        Button(memo.state == .archived ? "Restore" : "Archive") {
                            Task {
                                if memo.state == .archived { await viewModel.restoreMemo(memo.name) }
                                else { await viewModel.archiveMemo(memo.name) }
                            }
                        }
                        Divider()
                        Button("Delete", role: .destructive) {
                            Task { await viewModel.deleteMemo(memo.name) }
                        }
                    }
            }
            .listStyle(.plain)
            .searchable(text: $viewModel.searchText, prompt: "Search memos")
        }
        .navigationTitle("Memos")
        .task {
            // Sync on first appear so memos load even if app-launch syncAll()
            // ran before Memos credentials were configured.
            if viewModel.filteredMemos.isEmpty {
                await viewModel.sync()
            }
        }
        .toolbar {
            ToolbarItemGroup(placement: .primaryAction) {
                Button {
                    Task { await viewModel.sync() }
                } label: {
                    Label("Sync", systemImage: "arrow.clockwise")
                }
                .accessibilityLabel("Sync memos")
                .keyboardShortcut("r", modifiers: [.command, .shift])

                Button {
                    showCreateSheet = true
                } label: {
                    Label("New Memo", systemImage: "plus")
                }
                .accessibilityLabel("Create memo")
                .keyboardShortcut("n", modifiers: .command)
            }
        }
        .sheet(isPresented: $showCreateSheet) {
            CreateMemoSheetMac(viewModel: viewModel, initialText: captureInitialText)
        }
        .onChange(of: appViewModel.captureInitialText) { _, newText in
            guard let text = newText else { return }
            captureInitialText = text
            showCreateSheet = true
            appViewModel.consumeCaptureTrigger()
        }
        .onChange(of: viewModel.filteredMemos) { _, memos in
            // Auto-select first memo if none selected
            if appViewModel.selectedMemoID == nil, let first = memos.first {
                appViewModel.selectedMemoID = first.name
            }
        }
    }

    private func filterChip(_ title: String, isSelected: Bool, action: @escaping () -> Void) -> some View {
        Button(action: action) {
            Text(title)
                .font(.caption)
                .padding(.horizontal, 10)
                .padding(.vertical, 4)
                .background(isSelected ? Color.accentColor.opacity(0.2) : Color(.controlBackgroundColor))
                .foregroundColor(isSelected ? .accentColor : .primary)
                .clipShape(Capsule())
                .overlay(Capsule().stroke(isSelected ? Color.accentColor : Color.secondary.opacity(0.3), lineWidth: 0.5))
        }
        .buttonStyle(.plain)
        .accessibilityLabel(title)
        .accessibilityAddTraits(isSelected ? .isSelected : [])
    }
}

// ─── Row ──────────────────────────────────────────────────────────────────────

struct MemoListRow: View {
    let memo: MemosMemo

    var body: some View {
        VStack(alignment: .leading, spacing: 4) {
            if !memo.property.title.isEmpty {
                Text(memo.property.title)
                    .font(.headline)
                    .lineLimit(1)
            }
            Text(memo.snippet.isEmpty ? memo.content : memo.snippet)
                .font(.subheadline)
                .foregroundStyle(.secondary)
                .lineLimit(3)
            HStack(spacing: 4) {
                if memo.pinned {
                    Image(systemName: "pin.fill")
                        .font(.caption2)
                        .foregroundStyle(.orange)
                }
                // Visibility badge
                switch memo.visibility {
                case .public:
                    Label("public", systemImage: "globe")
                        .font(.caption2)
                        .foregroundStyle(.tint)
                        .labelStyle(.iconOnly)
                case .protected_:
                    Label("protected", systemImage: "person.2.fill")
                        .font(.caption2)
                        .foregroundStyle(.orange)
                        .labelStyle(.iconOnly)
                case .private:
                    Label("private", systemImage: "lock.fill")
                        .font(.caption2)
                        .foregroundStyle(.secondary)
                        .labelStyle(.iconOnly)
                }
                ForEach(memo.tags.prefix(3), id: \.self) { tag in
                    Text("#\(tag)")
                        .font(.caption2)
                        .foregroundStyle(.tint)
                }
                Spacer()
                Text(relativeTime(memo.displayTime))
                    .font(.caption2)
                    .foregroundStyle(.secondary)
            }
        }
        .padding(.vertical, 4)
        .accessibilityLabel(memo.property.title.isEmpty ? memo.snippet : memo.property.title)
    }

    private func relativeTime(_ date: Date) -> String {
        let diff = Int(Date().timeIntervalSince(date))
        switch diff {
        case ..<60:      return "just now"
        case ..<3600:    return "\(diff / 60)m ago"
        case ..<86400:   return "\(diff / 3600)h ago"
        default:
            let fmt = DateFormatter(); fmt.dateFormat = "MMM d"
            return fmt.string(from: date)
        }
    }
}

// ─── Create sheet (macOS) ─────────────────────────────────────────────────────

struct CreateMemoSheetMac: View {

    var viewModel: MemosViewModel
    var initialText: String = ""
    @Environment(\.dismiss) private var dismiss
    @State private var content = ""
    @State private var visibility: MemoVisibility = .private
    @State private var attachments: [MemoPendingAttachment] = []

    var body: some View {
        VStack(alignment: .leading, spacing: 16) {
            Text("New Memo")
                .font(.title2)
                .fontWeight(.semibold)

            TextEditor(text: $content)
                .font(.body)
                .frame(minHeight: 160)
                .overlay(RoundedRectangle(cornerRadius: 6).stroke(Color.secondary.opacity(0.3)))
                .onAppear {
                    if content.isEmpty && !initialText.isEmpty {
                        content = initialText
                    }
                }

            Picker("Visibility", selection: $visibility) {
                Text("Private").tag(MemoVisibility.private)
                Text("Protected").tag(MemoVisibility.protected_)
                Text("Public").tag(MemoVisibility.public)
            }
            .pickerStyle(.segmented)

            if !attachments.isEmpty {
                ScrollView(.horizontal) {
                    HStack {
                        ForEach(attachments, id: \.filename) { att in
                            HStack(spacing: 4) {
                                Image(systemName: "paperclip")
                                Text(att.filename).font(.caption)
                                Button { attachments.removeAll { $0.filename == att.filename } } label: {
                                    Image(systemName: "xmark.circle.fill").font(.caption)
                                }
                                .buttonStyle(.plain)
                            }
                            .padding(.horizontal, 8).padding(.vertical, 4)
                            .background(Color(.controlBackgroundColor))
                            .clipShape(Capsule())
                        }
                    }
                }
            }

            HStack {
                Button {
                    let panel = NSOpenPanel()
                    panel.allowsMultipleSelection = true
                    panel.canChooseFiles = true
                    panel.canChooseDirectories = false
                    if panel.runModal() == .OK {
                        for url in panel.urls {
                            if let data = try? Data(contentsOf: url) {
                                let mime = url.mimeType
                                attachments.append(MemoPendingAttachment(filename: url.lastPathComponent, mimeType: mime, data: data))
                            }
                        }
                    }
                } label: {
                    Label("Attach", systemImage: "paperclip")
                }

                Spacer()

                Button("Cancel", role: .cancel) { dismiss() }
                Button("Post") {
                    Task {
                        await viewModel.createMemo(content: content, visibility: visibility, attachments: attachments)
                        dismiss()
                    }
                }
                .buttonStyle(.borderedProminent)
                .disabled(content.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty)
            }
        }
        .padding(24)
        .frame(minWidth: 500, minHeight: 400)
    }
}

private extension URL {
    var mimeType: String {
        if let type = UTType(filenameExtension: pathExtension) {
            return type.preferredMIMEType ?? "application/octet-stream"
        }
        return "application/octet-stream"
    }
}
