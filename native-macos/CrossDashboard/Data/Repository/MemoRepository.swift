import Foundation
import os.log
import SwiftData
import CrossDashboardKit

/// Mirrors MemoRepository.kt.
@Observable
@MainActor
final class MemoRepository {

    private static let log = Logger(subsystem: "com.crossdashboard", category: "MemoRepository")

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
        let models = (try? context.fetch(FetchDescriptor<MemosModel>())) ?? []
        allMemos = models.map { $0.toDomain() }.sorted { $0.displayTime > $1.displayTime }
        Self.log.debug("[loadFromDB] \(self.allMemos.count) memos loaded from SwiftData")
    }

    // ─── Sync ─────────────────────────────────────────────────────────────────

    func syncMemos() async {
        guard client.baseUrl() != nil else {
            Self.log.warning("[syncMemos] Skipped — no Memos host configured")
            return
        }
        Self.log.info("[syncMemos] Starting sync")
        var fetched: [MemosMemo] = []

        for state in [MemoState.normal, .archived] {
            var pageToken: String? = nil
            repeat {
                let result = await client.listMemos(pageToken: pageToken, state: state)
                Self.log.info("[syncMemos] Fetched \(result.memos.count) \(state.rawValue, privacy: .public) memos (pageToken=\(pageToken ?? "nil", privacy: .public))")
                fetched.append(contentsOf: result.memos)
                pageToken = result.nextPageToken
            } while pageToken != nil
        }

        Self.log.info("[syncMemos] Total fetched: \(fetched.count)")
        guard !fetched.isEmpty else {
            Self.log.warning("[syncMemos] No memos returned from server — DB unchanged")
            return
        }
        do {
            try context.delete(model: MemosModel.self)
            try context.save()
        } catch {
            Self.log.error("[syncMemos] Failed to clear memos: \(error.localizedDescription, privacy: .public)")
            return
        }
        do {
            fetched.forEach { context.insert(MemosModel(from: $0)) }
            try context.save()
        } catch {
            Self.log.error("[syncMemos] Failed to save memos: \(error.localizedDescription, privacy: .public)")
            return
        }
        loadFromDB()
        Self.log.info("[syncMemos] Sync complete — \(self.allMemos.count) memos in allMemos")
    }

    // ─── Create ───────────────────────────────────────────────────────────────

    func createMemo(
        content: String,
        visibility: MemoVisibility,
        attachments: [MemoPendingAttachment]
    ) async -> MemosMemo? {
        print("[MemoRepository] createMemo: \(attachments.count) attachment(s)")
        var names: [String] = []
        for (i, att) in attachments.enumerated() {
            print("[MemoRepository]   uploading attachment[\(i)]: \(att.filename) \(att.mimeType) \(att.data.count) bytes")
            if let uploaded = await client.createAttachment(
                filename: att.filename,
                mimeType: att.mimeType,
                data: att.data
            ) {
                print("[MemoRepository]   attachment[\(i)] uploaded → \(uploaded.name)")
                names.append(uploaded.name)
            } else {
                print("[MemoRepository]   attachment[\(i)] upload FAILED")
            }
        }
        print("[MemoRepository] createMemo: calling client with attachmentNames=\(names)")
        guard let memo = await client.createMemo(
            content: content,
            visibility: visibility,
            attachmentNames: names
        ) else {
            print("[MemoRepository] createMemo: client.createMemo returned nil")
            return nil
        }
        print("[MemoRepository] createMemo: memo created — \(memo.name), attachments in domain model: \(memo.attachments.count)")
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
