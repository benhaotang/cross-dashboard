import Foundation
import SwiftData
import CrossDashboardKit

/// Mirrors MemoRepository.kt.
@Observable
@MainActor
final class MemoRepository {

    private let context: ModelContext
    private let client: MemosClient

    var allMemos: [MemosMemo] = []
    var normalMemos: [MemosMemo] { allMemos.filter { $0.state == .normal } }
    var archivedMemos: [MemosMemo] { allMemos.filter { $0.state == .archived } }

    init(context: ModelContext, client: MemosClient) {
        self.context = context
        self.client  = client
        loadFromDB()
    }

    // ─── DB ───────────────────────────────────────────────────────────────────

    func loadFromDB() {
        var descriptor = FetchDescriptor<MemosModel>()
        descriptor.includePendingChanges = false
        let models = (try? context.fetch(descriptor)) ?? []
        allMemos = models.map { $0.toDomain() }.sorted { $0.displayTime > $1.displayTime }
    }

    // ─── Sync ─────────────────────────────────────────────────────────────────

    @discardableResult
    func syncMemos() async -> Bool {
        guard client.baseUrl() != nil else { return true }
        var fetched: [MemosMemo] = []

        for state in [MemoState.normal, .archived] {
            var pageToken: String? = nil
            repeat {
                guard let result = await client.listMemosForSync(
                    pageToken: pageToken,
                    state: state
                ) else { return false }
                fetched.append(contentsOf: result.memos)
                pageToken = result.nextPageToken
            } while pageToken != nil
        }

        do {
            try context.transaction {
                try context.delete(model: MemosModel.self)
                fetched.forEach { context.insert(MemosModel(from: $0)) }
            }
        } catch {
            context.rollback()
            return false
        }
        loadFromDB()
        return true
    }

    // ─── Create ───────────────────────────────────────────────────────────────

    func createMemo(
        content: String,
        visibility: MemoVisibility,
        attachments: [MemoPendingAttachment]
    ) async -> MemosMemo? {
        var names: [String] = []
        for att in attachments {
            if let uploaded = await client.createAttachment(
                filename: att.filename,
                mimeType: att.mimeType,
                data: att.data
            ) {
                names.append(uploaded.name)
            }
        }
        guard let memo = await client.createMemo(
            content: content,
            visibility: visibility,
            attachmentNames: names
        ) else { return nil }
        context.insert(MemosModel(from: memo))
        try? context.save()
        loadFromDB()
        return memo
    }

    // ─── Delete ───────────────────────────────────────────────────────────────

    func deleteMemo(_ name: String, force: Bool = false) async {
        await client.deleteMemo(name, force: force)
        let models = (try? context.fetch(FetchDescriptor<MemosModel>())) ?? []
        models.filter { $0.name == name }.forEach { context.delete($0) }
        try? context.save()
        loadFromDB()
    }

    // ─── Archive / Restore ───────────────────────────────────────────────────

    func archiveMemo(_ name: String) async -> MemosMemo? {
        guard let updated = await client.updateMemo(name, state: .archived, updateMask: "state")
        else { return nil }
        upsertInDB(updated)
        return updated
    }

    func restoreMemo(_ name: String) async -> MemosMemo? {
        guard let updated = await client.updateMemo(name, state: .normal, updateMask: "state")
        else { return nil }
        upsertInDB(updated)
        return updated
    }

    // ─── Comments ─────────────────────────────────────────────────────────────

    func loadComments(_ memoId: String) async -> [MemosMemo] {
        await client.listMemoComments(memoId)
    }

    func createComment(parentId: String, content: String) async -> MemosMemo? {
        await client.createMemoComment(parentMemoId: parentId, content: content)
    }

    // ─── Share ────────────────────────────────────────────────────────────────

    func createShare(_ memoId: String) async -> String? {
        await client.createMemoShare(memoId)
    }

    func getMemoByShare(_ shareId: String) async -> MemosMemo? {
        await client.getMemoByShare(shareId)
    }

    // ─── Relations ────────────────────────────────────────────────────────────

    func listRelations(_ memoId: String) async -> [MemoRelationSwift] {
        await client.listMemoRelations(memoId)
    }

    // ─── Private ──────────────────────────────────────────────────────────────

    private func upsertInDB(_ memo: MemosMemo) {
        let models = (try? context.fetch(FetchDescriptor<MemosModel>())) ?? []
        if let existing = models.first(where: { $0.name == memo.name }) {
            context.delete(existing)
        }
        context.insert(MemosModel(from: memo))
        try? context.save()
        loadFromDB()
    }
}

// ─── Pending attachment for UI layer ─────────────────────────────────────────

struct MemoPendingAttachment: Sendable {
    let filename: String
    let mimeType: String
    let data: Data
}
