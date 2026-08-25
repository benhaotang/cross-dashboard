import Foundation
import SwiftData
import CrossDashboardKit

/// Mirrors NoteRepository.kt.
@Observable
@MainActor
final class NoteRepository {

    private let context: ModelContext
    private let client: CalDavClient

    var notes: [Note] = []

    init(context: ModelContext, client: CalDavClient) {
        self.context = context
        self.client  = client
        loadFromDB()
    }

    func loadFromDB() {
        var descriptor = FetchDescriptor<NoteModel>()
        descriptor.includePendingChanges = false
        let models = (try? context.fetch(descriptor)) ?? []
        notes = models.map { $0.toDomain() }.sorted { $0.lastModified > $1.lastModified }
    }

    @discardableResult
    func sync(calendarHrefs: [String]) async -> Bool {
        guard !calendarHrefs.isEmpty else { return true }
        guard let fresh = await client.fetchNotes(calendarHrefs: calendarHrefs) else { return false }
        do {
            try context.transaction {
                try context.delete(model: NoteModel.self)
                fresh.forEach { context.insert(NoteModel(from: $0)) }
            }
            loadFromDB()
            return true
        } catch {
            context.rollback()
            return false
        }
    }

    func create(_ note: Note, calendarHref: String) async throws -> Note {
        let saved = try await client.createNote(note, calendarHref: calendarHref)
        await MainActor.run {
            context.insert(NoteModel(from: saved))
            try? context.save()
            loadFromDB()
        }
        return saved
    }

    func update(_ note: Note) async throws {
        try await client.updateNote(note)
        await MainActor.run {
            let models = (try? context.fetch(FetchDescriptor<NoteModel>())) ?? []
            if let existing = models.first(where: { $0.uid == note.uid }) {
                context.delete(existing)
            }
            context.insert(NoteModel(from: note))
            try? context.save()
            loadFromDB()
        }
    }

    func delete(_ note: Note) async throws {
        try await client.deleteNote(note)
        await MainActor.run {
            let models = (try? context.fetch(FetchDescriptor<NoteModel>())) ?? []
            models.filter { $0.uid == note.uid }.forEach { context.delete($0) }
            try? context.save()
            loadFromDB()
        }
    }
}
