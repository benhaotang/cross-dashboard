import Foundation
import os.log
import CrossDashboardKit

/// Memos API client using URLSession + Codable DTOs.
/// Auth: Authorization: Bearer <token>
/// Direct Swift port of MemosClient.kt.
final class MemosClient: Sendable {

    private static let log = Logger(subsystem: "com.crossdashboard", category: "MemosClient")

    private let keychain: KeychainStore
    private let session: URLSession
    private let decoder: JSONDecoder
    private let encoder: JSONEncoder

    init(keychain: KeychainStore = .shared) {
        self.keychain = keychain
        let config = URLSessionConfiguration.default
        config.timeoutIntervalForRequest = 30
        config.timeoutIntervalForResource = 60
        self.session = URLSession(configuration: config)

        let dec = JSONDecoder()
        dec.dateDecodingStrategy = .iso8601
        self.decoder = dec

        let enc = JSONEncoder()
        enc.dateEncodingStrategy = .iso8601
        self.encoder = enc
    }

    // ─── Memos list ───────────────────────────────────────────────────────────

    func listMemos(
        pageToken: String? = nil,
        filter: String? = nil,
        state: MemoState = .normal,
        orderBy: String = "display_time desc",
        pageSize: Int = 50
    ) async -> (memos: [MemosMemo], nextPageToken: String?) {
        guard let base = baseUrl() else {
            Self.log.warning("[listMemos] baseUrl is nil — Memos host not configured")
            return ([], nil)
        }
        var comps = URLComponents(string: "\(base)/api/v1/memos")!
        var items: [URLQueryItem] = [
            URLQueryItem(name: "pageSize", value: "\(pageSize)"),
            URLQueryItem(name: "orderBy", value: orderBy),
        ]
        if state != .normal { items.append(URLQueryItem(name: "state", value: state.rawValue)) }
        if let f = filter  { items.append(URLQueryItem(name: "filter", value: f)) }
        if let t = pageToken { items.append(URLQueryItem(name: "pageToken", value: t)) }
        comps.queryItems = items
        guard let url = comps.url else {
            Self.log.error("[listMemos] Failed to build URL from components")
            return ([], nil)
        }
        Self.log.debug("[listMemos] GET \(url.absoluteString, privacy: .public)")
        guard let data = await get(url.absoluteString) else {
            Self.log.error("[listMemos] HTTP request failed or returned non-2xx for \(url.absoluteString, privacy: .public)")
            return ([], nil)
        }
        Self.log.debug("[listMemos] Response body (\(data.count) bytes): \(String(data: data, encoding: .utf8) ?? "<non-UTF8>", privacy: .public)")
        guard let dto = try? decoder.decode(MemoListDto.self, from: data) else {
            // Decode failed — log the raw JSON to surface field mismatches
            let raw = String(data: data, encoding: .utf8) ?? "<non-UTF8>"
            Self.log.error("[listMemos] JSON decode failed. Raw response: \(raw, privacy: .public)")
            return ([], nil)
        }
        let memos = dto.memos.compactMap { $0.toDomain() }
        Self.log.info("[listMemos] Decoded \(memos.count) memos (state=\(state.rawValue, privacy: .public)), nextPageToken=\(dto.nextPageToken ?? "nil", privacy: .public)")
        return (memos, dto.nextPageToken.flatMap { $0.isEmpty ? nil : $0 })
    }

    func getMemo(_ memoId: String) async -> MemosMemo? {
        guard let base = baseUrl() else { return nil }
        guard let data = await get("\(base)/api/v1/\(memoId)") else { return nil }
        return try? decoder.decode(MemoDtoFull.self, from: data).toDomain()
    }

    // ─── Create / Update / Delete ─────────────────────────────────────────────

    func createMemo(
        content: String,
        visibility: MemoVisibility = .private,
        attachmentNames: [String] = []
    ) async -> MemosMemo? {
        guard let base = baseUrl() else { return nil }
        var body: [String: Any] = [
            "state": "NORMAL",
            "content": content,
            "visibility": visibility.rawValue,
        ]
        if !attachmentNames.isEmpty {
            body["attachments"] = attachmentNames.map { ["name": $0] }
        }
        guard let data = await postJSON("\(base)/api/v1/memos", body: body) else { return nil }
        return try? decoder.decode(MemoDtoFull.self, from: data).toDomain()
    }

    func updateMemo(
        _ memoId: String,
        content: String? = nil,
        state: MemoState? = nil,
        updateMask: String
    ) async -> MemosMemo? {
        guard let base = baseUrl() else { return nil }
        var body: [String: Any] = [:]
        if let c = content { body["content"] = c }
        if let s = state   { body["state"] = s.rawValue }
        guard !body.isEmpty else { return nil }
        let urlStr = "\(base)/api/v1/\(memoId)?updateMask=\(updateMask.urlEncoded)"
        guard let data = await patchJSON(urlStr, body: body) else { return nil }
        return try? decoder.decode(MemoDtoFull.self, from: data).toDomain()
    }

    @discardableResult
    func deleteMemo(_ memoId: String, force: Bool = false) async -> Bool {
        guard let base = baseUrl() else { return false }
        let url = "\(base)/api/v1/\(memoId)" + (force ? "?force=true" : "")
        return await delete(url)
    }

    // ─── Comments ─────────────────────────────────────────────────────────────

    func listMemoComments(_ memoId: String, pageToken: String? = nil) async -> [MemosMemo] {
        guard let base = baseUrl() else { return [] }
        var url = "\(base)/api/v1/\(memoId)/comments"
        if let t = pageToken { url += "?pageToken=\(t.urlEncoded)" }
        guard let data = await get(url) else { return [] }
        let dto = try? decoder.decode(MemoListDto.self, from: data)
        return dto?.memos.compactMap { $0.toDomain() } ?? []
    }

    func createMemoComment(
        parentMemoId: String,
        content: String,
        visibility: MemoVisibility = .private
    ) async -> MemosMemo? {
        guard let base = baseUrl() else { return nil }
        let body: [String: Any] = [
            "state": "NORMAL",
            "content": content,
            "visibility": visibility.rawValue,
        ]
        guard let data = await postJSON("\(base)/api/v1/\(parentMemoId)/comments", body: body)
        else { return nil }
        return try? decoder.decode(MemoDtoFull.self, from: data).toDomain()
    }

    // ─── Attachments ──────────────────────────────────────────────────────────

    func listMemoAttachments(_ memoId: String) async -> [MemosAttachment] {
        guard let base = baseUrl() else { return [] }
        guard let data = await get("\(base)/api/v1/\(memoId)/attachments") else { return [] }
        let dto = try? decoder.decode(AttachmentListDto.self, from: data)
        return (dto?.attachments ?? []).map { $0.toDomain() }
    }

    /// Uploads a file; content is Base64-encoded in the JSON body (no multipart).
    func createAttachment(
        filename: String,
        mimeType: String,
        data fileData: Data,
        memoName: String? = nil
    ) async -> MemosAttachment? {
        guard let base = baseUrl() else { return nil }
        var body: [String: Any] = [
            "filename": filename,
            "type": mimeType,
            "content": fileData.base64EncodedString(),
        ]
        if let m = memoName { body["memo"] = m }
        guard let data = await postJSON("\(base)/api/v1/attachments", body: body) else { return nil }
        return try? decoder.decode(AttachmentDto.self, from: data).toDomain()
    }

    @discardableResult
    func deleteAttachments(_ names: [String]) async -> Bool {
        guard let base = baseUrl() else { return false }
        let body: [String: Any] = ["names": names]
        return await postJSON("\(base)/api/v1/attachments:batchDelete", body: body) != nil
    }

    // ─── Relations ────────────────────────────────────────────────────────────

    func listMemoRelations(_ memoId: String) async -> [MemoRelationSwift] {
        guard let base = baseUrl() else { return [] }
        guard let data = await get("\(base)/api/v1/\(memoId)/relations") else { return [] }
        let dto = try? decoder.decode(RelationListDto.self, from: data)
        return dto?.relations.map {
            MemoRelationSwift(
                memoName: $0.memo?.name ?? "",
                memoSnippet: $0.memo?.snippet ?? "",
                relatedMemoName: $0.relatedMemo?.name ?? "",
                relatedMemoSnippet: $0.relatedMemo?.snippet ?? ""
            )
        } ?? []
    }

    // ─── Shares ───────────────────────────────────────────────────────────────

    /// Creates a share link and returns the full share URL: {host}/s/{token}
    func createMemoShare(_ memoId: String, expireTime: Date? = nil) async -> String? {
        guard let base = baseUrl() else { return nil }
        var body: [String: Any] = [:]
        if let exp = expireTime {
            let iso = ISO8601DateFormatter().string(from: exp)
            body["expireTime"] = iso
        }
        guard let data = await postJSON("\(base)/api/v1/\(memoId)/shares", body: body) else { return nil }
        let dto = try? decoder.decode(ShareDto.self, from: data)
        guard let token = dto?.name.components(separatedBy: "/").last, !token.isEmpty else { return nil }
        return "\(base)/s/\(token)"
    }

    // ─── Share resolution (no auth) ───────────────────────────────────────────

    func getMemoByShare(_ shareId: String) async -> MemosMemo? {
        guard let base = baseUrl() else { return nil }
        var req = URLRequest(url: URL(string: "\(base)/api/v1/shares/\(shareId)")!)
        req.timeoutInterval = 30
        guard let (data, resp) = try? await session.data(for: req),
              (resp as? HTTPURLResponse)?.statusCode == 200 else { return nil }
        return try? decoder.decode(MemoDtoFull.self, from: data).toDomain()
    }

    // ─── Base URL + auth ──────────────────────────────────────────────────────

    func baseUrl() -> String? {
        keychain.get(CredentialKey.memosHost)?.trimmingCharacters(in: CharacterSet(charactersIn: "/"))
    }

    private func addBearer(_ req: inout URLRequest) {
        if let token = keychain.get(CredentialKey.memosToken) {
            req.setValue("Bearer \(token)", forHTTPHeaderField: "Authorization")
        }
    }

    // ─── HTTP helpers ─────────────────────────────────────────────────────────

    private func get(_ urlStr: String) async -> Data? {
        guard let url = URL(string: urlStr) else {
            Self.log.error("[get] Invalid URL: \(urlStr, privacy: .public)")
            return nil
        }
        var req = URLRequest(url: url)
        req.timeoutInterval = 30
        addBearer(&req)
        do {
            let (data, resp) = try await session.data(for: req)
            let status = (resp as? HTTPURLResponse)?.statusCode ?? -1
            guard (200..<300).contains(status) else {
                let body = String(data: data, encoding: .utf8) ?? "<non-UTF8>"
                Self.log.error("[get] HTTP \(status) for \(urlStr, privacy: .public) — body: \(body, privacy: .public)")
                return nil
            }
            return data
        } catch {
            Self.log.error("[get] Request error for \(urlStr, privacy: .public): \(error.localizedDescription, privacy: .public)")
            return nil
        }
    }

    private func postJSON(_ urlStr: String, body: [String: Any]) async -> Data? {
        guard let url = URL(string: urlStr),
              let bodyData = try? JSONSerialization.data(withJSONObject: body) else { return nil }
        var req = URLRequest(url: url)
        req.httpMethod = "POST"
        req.setValue("application/json", forHTTPHeaderField: "Content-Type")
        req.httpBody = bodyData
        req.timeoutInterval = 60
        addBearer(&req)
        guard let (data, resp) = try? await session.data(for: req),
              (resp as? HTTPURLResponse).map({ (200..<300).contains($0.statusCode) }) == true
        else { return nil }
        return data
    }

    private func patchJSON(_ urlStr: String, body: [String: Any]) async -> Data? {
        guard let url = URL(string: urlStr),
              let bodyData = try? JSONSerialization.data(withJSONObject: body) else { return nil }
        var req = URLRequest(url: url)
        req.httpMethod = "PATCH"
        req.setValue("application/json", forHTTPHeaderField: "Content-Type")
        req.httpBody = bodyData
        req.timeoutInterval = 60
        addBearer(&req)
        guard let (data, resp) = try? await session.data(for: req),
              (resp as? HTTPURLResponse).map({ (200..<300).contains($0.statusCode) }) == true
        else { return nil }
        return data
    }

    private func delete(_ urlStr: String) async -> Bool {
        guard let url = URL(string: urlStr) else { return false }
        var req = URLRequest(url: url)
        req.httpMethod = "DELETE"
        req.timeoutInterval = 30
        addBearer(&req)
        guard let (_, resp) = try? await session.data(for: req) else { return false }
        return (resp as? HTTPURLResponse).map { (200..<300).contains($0.statusCode) } ?? false
    }

    // ─── DTOs ─────────────────────────────────────────────────────────────────

    private struct MemoListDto: Decodable {
        var memos: [MemoDtoFull] = []
        var nextPageToken: String?
    }

    private struct MemoDtoFull: Decodable {
        var name: String = ""
        var state: String = "STATE_UNSPECIFIED"
        var content: String = ""
        var visibility: String = "VISIBILITY_UNSPECIFIED"
        // proto3 JSON omits fields that are the zero value, and some server
        // versions return `null` for unset repeated / message fields.
        // Using optional + nil-coalescing prevents the entire MemoListDto
        // from failing to decode when any one memo has a null field.
        var tags: [String]? = nil
        var pinned: Bool = false
        var attachments: [AttachmentDto]? = nil
        var property: MemoPropertyDto? = nil
        var snippet: String = ""
        var createTime: String = ""
        var displayTime: String = ""
        var updateTime: String = ""

        func toDomain() -> MemosMemo {
            let fmt = ISO8601DateFormatter()
            // Some servers use fractional seconds; try with and without.
            func parse(_ s: String) -> Date {
                if let d = fmt.date(from: s) { return d }
                let alt = ISO8601DateFormatter()
                alt.formatOptions = [.withInternetDateTime, .withFractionalSeconds]
                return alt.date(from: s) ?? Date()
            }
            return MemosMemo(
                name: name,
                state: state == "ARCHIVED" ? .archived : .normal,
                content: content,
                visibility: {
                    switch visibility {
                    case "PUBLIC":    return .public
                    case "PROTECTED": return .protected_
                    default:          return .private
                    }
                }(),
                tags: tags ?? [],
                pinned: pinned,
                attachments: (attachments ?? []).map { $0.toDomain() },
                property: property?.toDomain() ?? MemoProperty(),
                snippet: snippet,
                createTime:  parse(createTime),
                displayTime: parse(displayTime),
                updateTime:  parse(updateTime)
            )
        }
    }

    fileprivate struct AttachmentDto: Decodable {
        var name: String = ""
        var filename: String = ""
        var externalLink: String = ""
        var type: String = ""
        var size: String = "0"
        var memo: String = ""

        func toDomain() -> MemosAttachment {
            MemosAttachment(
                name: name,
                filename: filename,
                externalLink: externalLink,
                type: type,
                size: Int64(size) ?? 0,
                memo: memo
            )
        }
    }

    private struct AttachmentListDto: Decodable {
        var attachments: [AttachmentDto]? = nil
    }

    private struct MemoPropertyDto: Decodable {
        // All Bool flags have server-side defaults; the API often omits them.
        var hasLink: Bool?
        var hasTaskList: Bool?
        var hasIncompleteTasks: Bool?
        // `title` is absent when the memo has no explicit title (very common).
        var title: String?

        func toDomain() -> MemoProperty {
            MemoProperty(
                hasLink: hasLink ?? false,
                hasTaskList: hasTaskList ?? false,
                hasIncompleteTasks: hasIncompleteTasks ?? false,
                title: title ?? ""
            )
        }
    }

    private struct RelationListDto: Decodable {
        var relations: [RelationDto] = []
    }

    private struct RelationDto: Decodable {
        var memo: RelationMemoDto?
        var relatedMemo: RelationMemoDto?
        var type: String = ""
    }

    private struct RelationMemoDto: Decodable {
        var name: String = ""
        var snippet: String = ""
    }

    private struct ShareDto: Decodable {
        var name: String = ""
    }
}

/// Lightweight relation returned from listMemoRelations.
struct MemoRelationSwift: Sendable {
    let memoName: String
    let memoSnippet: String
    let relatedMemoName: String
    let relatedMemoSnippet: String
}


private extension String {
    var urlEncoded: String {
        addingPercentEncoding(withAllowedCharacters: .urlQueryAllowed) ?? self
    }
}
