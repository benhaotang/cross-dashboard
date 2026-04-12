import SwiftUI
import CrossDashboardKit

/// Note detail panel with read/edit modes.
/// Mirrors NoteReadView / NoteEditForm on Android.
struct NoteDetailView: View {

    @Environment(\.appContainer) private var container
    let noteID: String?

    @State private var isEditing: Bool = false
    @State private var editSummary: String = ""
    @State private var editBody: String = ""
    @State private var editCategories: String = ""
    @State private var isSaving: Bool = false
    @State private var errorMessage: String?

    private var note: Note? {
        guard let id = noteID else { return nil }
        return container.noteRepository.notes.first { $0.uid == id }
    }

    var body: some View {
        if let note {
            PropertyDetailShell(title: isEditing ? "Edit Note" : note.summary) {
                if isEditing {
                    editForm(note)
                } else {
                    readContent(note)
                }
            }
            .toolbar {
                if isEditing {
                    ToolbarItem(placement: .cancellationAction) {
                        Button("Cancel") {
                            isEditing = false
                        }
                    }
                    ToolbarItem(placement: .confirmationAction) {
                        Button("Save") {
                            saveEdits(note)
                        }
                        .disabled(isSaving || editSummary.trimmingCharacters(in: .whitespaces).isEmpty)
                    }
                } else {
                    ToolbarItem(placement: .primaryAction) {
                        Button("Edit") {
                            loadForEdit(note)
                        }
                        .accessibilityLabel("Edit note")
                    }
                }
            }
            .alert("Error", isPresented: Binding(
                get: { errorMessage != nil },
                set: { if !$0 { errorMessage = nil } }
            )) {
                Button("OK") { errorMessage = nil }
            } message: {
                Text(errorMessage ?? "")
            }
            .onChange(of: noteID) { _, _ in
                isEditing = false
            }
        } else {
            ContentUnavailableView(
                "Select a note",
                systemImage: "note.text",
                description: Text("Choose a note from the grid to see its contents.")
            )
        }
    }

    // ─── Read view ────────────────────────────────────────────────────────────

    @ViewBuilder
    private func readContent(_ note: Note) -> some View {
        if !note.categories.isEmpty {
            HStack(spacing: 6) {
                ForEach(note.categories, id: \.self) { TagChip(tag: $0) }
            }
        }

        if !note.body.isEmpty {
            Divider()
            ReadMarkdownView(content: note.body)
        }

        Divider()

        ReadField(label: "Created") {
            Text(note.created, style: .date)
        }
        ReadField(label: "Modified") {
            Text(note.lastModified, style: .relative)
        }
    }

    // ─── Edit form ────────────────────────────────────────────────────────────

    @ViewBuilder
    private func editForm(_ note: Note) -> some View {
        Form {
            Section("Note") {
                TextField("Title", text: $editSummary)
                TextEditor(text: $editBody)
                    .frame(minHeight: 200)
            }
            Section("Tags (comma-separated)") {
                TextField("e.g. ideas, work", text: $editCategories)
            }
        }
        .formStyle(.grouped)
    }

    // ─── Helpers ──────────────────────────────────────────────────────────────

    private func loadForEdit(_ note: Note) {
        editSummary = note.summary
        editBody = note.body
        editCategories = note.categories.joined(separator: ", ")
        isEditing = true
    }

    private func saveEdits(_ note: Note) {
        let cats = editCategories
            .split(separator: ",")
            .map { $0.trimmingCharacters(in: .whitespaces) }
            .filter { !$0.isEmpty }

        let rebuilt = Note(
            uid: note.uid,
            summary: editSummary,
            body: editBody,
            categories: cats,
            created: note.created,
            lastModified: Date(),
            calendarHref: note.calendarHref,
            etag: note.etag,
            href: note.href
        )

        isSaving = true
        Task {
            defer { isSaving = false }
            do {
                try await container.noteRepository.update(rebuilt)
                isEditing = false
            } catch {
                errorMessage = error.localizedDescription
            }
        }
    }
}
