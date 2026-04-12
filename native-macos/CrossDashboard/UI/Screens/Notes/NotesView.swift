import SwiftUI
import CrossDashboardKit

/// Note grid/list with search and CRUD. Mirrors NotesScreen on Android.
struct NotesView: View {

    @Environment(AppViewModel.self) private var appViewModel
    @State private var viewModel = NotesViewModel()

    var body: some View {
        @Bindable var vm = viewModel
        Group {
            if viewModel.isLoading && viewModel.allNotes.isEmpty {
                ProgressView("Loading notes…")
                    .frame(maxWidth: .infinity, maxHeight: .infinity)
            } else if viewModel.filteredNotes.isEmpty {
                ContentUnavailableView(
                    viewModel.searchText.isEmpty ? "No notes" : "No results",
                    systemImage: "note.text",
                    description: Text(
                        viewModel.searchText.isEmpty
                            ? "Create your first note with ⌘N or the + button."
                            : "No notes match '\(viewModel.searchText)'."
                    )
                )
            } else {
                noteGrid
            }
        }
        .navigationTitle("Notes")
        .searchable(text: $vm.searchText, prompt: "Search notes")
        .toolbar { toolbarContent }
        .sheet(isPresented: $vm.isCreating) {
            createSheet
                .interactiveDismissDisabled(
                    !viewModel.editingSummary.isEmpty || !viewModel.editingBody.isEmpty
                )
        }
        .onChange(of: viewModel.selectedNoteID) { _, id in
            appViewModel.selectedNoteID = id
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

    // ─── Note grid ────────────────────────────────────────────────────────────

    private var noteGrid: some View {
        ScrollView {
            LazyVGrid(
                columns: [GridItem(.adaptive(minimum: 200, maximum: 280), spacing: 12)],
                spacing: 12
            ) {
                ForEach(viewModel.filteredNotes) { note in
                    NoteCard(note: note, isSelected: viewModel.selectedNoteID == note.uid) {
                        viewModel.selectedNoteID = note.uid
                    }
                    .contextMenu {
                        Button(role: .destructive) {
                            viewModel.delete(note)
                        } label: {
                            Label("Delete", systemImage: "trash")
                        }
                    }
                    .keyboardShortcut(.delete, modifiers: .command)
                }
            }
            .padding()
        }
    }

    // ─── Create sheet ─────────────────────────────────────────────────────────

    private var createSheet: some View {
        @Bindable var vm = viewModel
        return NavigationStack {
            Form {
                Section("Note") {
                    TextField("Title", text: $vm.editingSummary)
                    TextEditor(text: $vm.editingBody)
                        .frame(minHeight: 120)
                }
                Section("Tags (comma-separated)") {
                    TextField("e.g. ideas, work", text: $vm.editingCategories)
                }
            }
            .formStyle(.grouped)
            .navigationTitle("New Note")
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button("Cancel") { viewModel.cancelCreate() }
                }
                ToolbarItem(placement: .confirmationAction) {
                    Button("Create") { viewModel.submitCreate() }
                        .disabled(viewModel.editingSummary.trimmingCharacters(in: .whitespaces).isEmpty)
                }
            }
        }
        .frame(minWidth: 420, minHeight: 340)
    }

    // ─── Toolbar ──────────────────────────────────────────────────────────────

    @ToolbarContentBuilder
    private var toolbarContent: some ToolbarContent {
        ToolbarItem(placement: .primaryAction) {
            Button {
                viewModel.startCreate()
            } label: {
                Label("New Note", systemImage: "plus")
            }
            .keyboardShortcut("n", modifiers: .command)
            .accessibilityLabel("Create note")
        }
        ToolbarItem {
            Button {
                Task { await viewModel.sync() }
            } label: {
                Label("Sync", systemImage: "arrow.clockwise")
            }
            .accessibilityLabel("Sync notes")
        }
    }
}

// ─── NoteCard ─────────────────────────────────────────────────────────────────

private struct NoteCard: View {
    let note: Note
    let isSelected: Bool
    let onTap: () -> Void

    var body: some View {
        Button(action: onTap) {
            VStack(alignment: .leading, spacing: 6) {
                Text(note.summary)
                    .font(.headline)
                    .lineLimit(2)
                    .foregroundStyle(.primary)

                if !note.body.isEmpty {
                    Text(note.body)
                        .font(.caption)
                        .foregroundStyle(.secondary)
                        .lineLimit(4)
                }

                Spacer(minLength: 0)

                HStack {
                    if !note.categories.isEmpty {
                        ForEach(note.categories.prefix(2), id: \.self) { cat in
                            TagChip(tag: cat)
                        }
                    }
                    Spacer()
                    Text(note.lastModified, style: .relative)
                        .font(.caption2)
                        .foregroundStyle(.tertiary)
                }
            }
            .padding(12)
            .frame(maxWidth: .infinity, minHeight: 120, alignment: .topLeading)
            .background(
                RoundedRectangle(cornerRadius: 10)
                    .fill(isSelected ? Color.accentColor.opacity(0.15) : Color(.windowBackgroundColor))
                    .shadow(color: Color(.shadowColor).opacity(0.15), radius: 4, y: 2)
            )
            .overlay(
                RoundedRectangle(cornerRadius: 10)
                    .strokeBorder(isSelected ? Color.accentColor : Color(.separatorColor), lineWidth: isSelected ? 1.5 : 0.5)
            )
        }
        .buttonStyle(.plain)
        .accessibilityLabel(note.summary)
        .accessibilityValue(note.body.isEmpty ? "Empty note" : note.body.prefix(80).description)
        .accessibilityHint("Open note")
    }
}
