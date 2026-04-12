import Foundation
import Observation
import CrossDashboardKit

@Observable
@MainActor
final class NotesViewModel {

    var searchText: String = ""
    var selectedNoteID: String?
    var isLoading: Bool = false
    var errorMessage: String?

    // Edit state
    var isCreating: Bool = false
    var editingSummary: String = ""
    var editingBody: String = ""
    var editingCategories: String = ""

    // ─── Dependencies ─────────────────────────────────────────────────────────

    private let container: AppContainer

    init(container: AppContainer = .shared) {
        self.container = container
    }

    // ─── Derived data ─────────────────────────────────────────────────────────

    var allNotes: [Note] { container.noteRepository.notes }

    var filteredNotes: [Note] {
        guard !searchText.isEmpty else {
            return allNotes.sorted { $0.lastModified > $1.lastModified }
        }
        let q = searchText.lowercased()
        return allNotes
            .filter { $0.summary.lowercased().contains(q) || $0.body.lowercased().contains(q) }
            .sorted { $0.lastModified > $1.lastModified }
    }

    var selectedNote: Note? {
        guard let id = selectedNoteID else { return nil }
        return allNotes.first { $0.uid == id }
    }

    // ─── Actions ──────────────────────────────────────────────────────────────

    func sync() async {
        isLoading = true
        defer { isLoading = false }
        await container.syncAll()
    }

    func startCreate() {
        isCreating = true
        editingSummary = ""
        editingBody = ""
        editingCategories = ""
    }

    func cancelCreate() {
        isCreating = false
    }

    func submitCreate() {
        let calendarHref = container.keychain.get(CredentialKey.caldavDefaultTaskCalendar) ?? ""
        guard !editingSummary.trimmingCharacters(in: .whitespaces).isEmpty else { return }

        let cats = editingCategories
            .split(separator: ",")
            .map { $0.trimmingCharacters(in: .whitespaces) }
            .filter { !$0.isEmpty }

        let summary = editingSummary
        let body = editingBody
        isCreating = false
        editingSummary = ""
        editingBody = ""
        editingCategories = ""

        Task {
            do {
                let note = Note(
                    summary: summary,
                    body: body,
                    categories: cats,
                    calendarHref: calendarHref.isEmpty ? nil : calendarHref
                )
                let created = try await container.noteRepository.create(note, calendarHref: calendarHref)
                selectedNoteID = created.uid
            } catch {
                errorMessage = error.localizedDescription
            }
        }
    }

    func update(_ note: Note, summary: String, body: String, categories: [String]) {
        let rebuilt = Note(
            uid: note.uid,
            summary: summary,
            body: body,
            categories: categories,
            created: note.created,
            lastModified: Date(),
            calendarHref: note.calendarHref,
            etag: note.etag,
            href: note.href
        )
        Task {
            do {
                try await container.noteRepository.update(rebuilt)
            } catch {
                errorMessage = error.localizedDescription
            }
        }
    }

    func delete(_ note: Note) {
        if selectedNoteID == note.uid { selectedNoteID = nil }
        Task {
            do {
                try await container.noteRepository.delete(note)
            } catch {
                errorMessage = error.localizedDescription
            }
        }
    }
}
