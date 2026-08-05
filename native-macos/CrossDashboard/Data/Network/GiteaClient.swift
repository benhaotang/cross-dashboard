import Foundation
import CrossDashboardKit

/// Gitea API client using URLSession + Codable DTOs.
///
/// Direct Swift port of GiteaClient.kt.
final class GiteaClient: Sendable {

    private let keychain: KeychainStore
    private let session: URLSession
    private let decoder: JSONDecoder

    init(keychain: KeychainStore = .shared) {
        self.keychain = keychain
        let config = URLSessionConfiguration.default
        config.timeoutIntervalForRequest = 30
        self.session = URLSession(configuration: config)

        let dec = JSONDecoder()
        dec.dateDecodingStrategy = .iso8601
        self.decoder = dec
    }

    // ─── Issues ───────────────────────────────────────────────────────────────

    func fetchIssues(repositories: [String], state: String = "open") async -> [GiteaIssue] {
        guard let base = instanceUrl() else { return [] }
        var results: [GiteaIssue] = []
        for repo in repositories {
            var page = 1
            while true {
                let url = "\(base)/api/v1/repos/\(repo)/issues?state=\(state)&type=issues&limit=50&page=\(page)"
                guard let data = await get(url) else { break }
                do {
                    let dtos = try decoder.decode([GiteaIssueDto].self, from: data)
                    if dtos.isEmpty { break }
                    results += dtos.map { $0.toDomain(repo: repo) }
                    page += 1
                    if dtos.count < 50 { break }
                } catch {
                    break
                }
            }
        }
        return results
    }

    func updateIssue(
        repo: String,
        number: Int,
        title: String? = nil,
        body: String? = nil,
        state: String? = nil
    ) async throws -> GiteaIssue {
        guard let base = instanceUrl() else { throw GiteaError.noInstance }
        var parts: [String] = []
        if let t = title { parts.append(#""title":\#(jsonString(t))"#) }
        if let b = body  { parts.append(#""body":\#(jsonString(b))"#) }
        if let s = state { parts.append(#""state":"\#(s)""#) }
        let payload = "{\(parts.joined(separator: ","))}"
        guard let data = await patch("\(base)/api/v1/repos/\(repo)/issues/\(number)", body: payload)
        else { throw GiteaError.requestFailed }
        return try decoder.decode(GiteaIssueDto.self, from: data).toDomain(repo: repo)
    }

    func fetchComments(repo: String, number: Int) async -> [GiteaComment] {
        guard let base = instanceUrl() else { return [] }
        guard let data = await get("\(base)/api/v1/repos/\(repo)/issues/\(number)/comments") else { return [] }
        return (try? decoder.decode([GiteaCommentDto].self, from: data))?.map { $0.toDomain() } ?? []
    }

    func addComment(repo: String, number: Int, body: String) async throws -> GiteaComment {
        guard let base = instanceUrl() else { throw GiteaError.noInstance }
        let payload = #"{"body":\#(jsonString(body))}"#
        guard let data = await post("\(base)/api/v1/repos/\(repo)/issues/\(number)/comments", body: payload)
        else { throw GiteaError.requestFailed }
        return try decoder.decode(GiteaCommentDto.self, from: data).toDomain()
    }

    func fetchLabels(repo: String) async -> [GiteaLabel] {
        guard let base = instanceUrl() else { return [] }
        var labels: [GiteaLabel] = []
        for page in 1...100 {
            guard let data = await get("\(base)/api/v1/repos/\(repo)/labels?page=\(page)&limit=50"),
                  let batch = try? decoder.decode([GiteaLabelDto].self, from: data),
                  !batch.isEmpty else { break }
            labels.append(contentsOf: batch.map { GiteaLabel(id: $0.id, name: $0.name, color: $0.color) })
        }
        return labels
    }

    func createRepoLabel(repo: String, name: String, color: String = "0075ca") async throws -> GiteaLabel {
        guard let base = instanceUrl() else { throw GiteaError.noInstance }
        let payload = #"{"name":\#(jsonString(name)),"color":\#(jsonString("#" + color))}"#
        guard let data = await post("\(base)/api/v1/repos/\(repo)/labels", body: payload)
        else { throw GiteaError.requestFailed }
        let dto = try decoder.decode(GiteaLabelDto.self, from: data)
        return GiteaLabel(id: dto.id, name: dto.name, color: dto.color)
    }

    func replaceIssueLabels(repo: String, number: Int, labelIds: [Int64]) async throws {
        guard let base = instanceUrl() else { throw GiteaError.noInstance }
        let payload = #"{"labels":[\#(labelIds.map(String.init).joined(separator: ","))]}"#
        guard await put("\(base)/api/v1/repos/\(repo)/issues/\(number)/labels", body: payload) != nil
        else { throw GiteaError.requestFailed }
    }

    func fetchIssueAttachments(repo: String, issueNumber: Int) async -> [GiteaAttachment] {
        guard let base = instanceUrl() else { return [] }
        guard let data = await get("\(base)/api/v1/repos/\(repo)/issues/\(issueNumber)/assets") else { return [] }
        return (try? decoder.decode([GiteaAttachmentDto].self, from: data))?.map { $0.toDomain() } ?? []
    }

    func fetchCommentAttachments(repo: String, commentId: Int64) async -> [GiteaAttachment] {
        guard let base = instanceUrl() else { return [] }
        guard let data = await get("\(base)/api/v1/repos/\(repo)/issues/comments/\(commentId)/assets") else { return [] }
        return (try? decoder.decode([GiteaAttachmentDto].self, from: data))?.map { $0.toDomain() } ?? []
    }

    func createIssue(repo: String, title: String, body: String) async throws -> GiteaIssue {
        guard let base = instanceUrl() else { throw GiteaError.noInstance }
        var payload = #"{"title":\#(jsonString(title))"#
        if !body.isEmpty { payload += #","body":\#(jsonString(body))"# }
        payload += "}"
        guard let data = await post("\(base)/api/v1/repos/\(repo)/issues", body: payload)
        else { throw GiteaError.requestFailed }
        return try decoder.decode(GiteaIssueDto.self, from: data).toDomain(repo: repo)
    }

    func uploadIssueAttachment(
        repo: String,
        issueNumber: Int,
        fileName: String,
        bytes: Data,
        mimeType: String
    ) async throws -> String {
        guard let base = instanceUrl() else { throw GiteaError.noInstance }
        let urlStr = "\(base)/api/v1/repos/\(repo)/issues/\(issueNumber)/assets"
        let data = try await uploadMultipart(url: urlStr, fileName: fileName, bytes: bytes, mimeType: mimeType)
        let dto = try decoder.decode(GiteaAttachmentDto.self, from: data)
        return dto.browser_download_url
    }

    func uploadCommentAttachment(
        repo: String,
        commentId: Int64,
        fileName: String,
        bytes: Data,
        mimeType: String
    ) async throws -> String {
        guard let base = instanceUrl() else { throw GiteaError.noInstance }
        let urlStr = "\(base)/api/v1/repos/\(repo)/issues/comments/\(commentId)/assets"
        let data = try await uploadMultipart(url: urlStr, fileName: fileName, bytes: bytes, mimeType: mimeType)
        let dto = try decoder.decode(GiteaAttachmentDto.self, from: data)
        return dto.browser_download_url
    }

    // ─── HTTP helpers ─────────────────────────────────────────────────────────

    private func get(_ url: String) async -> Data? {
        guard let reqUrl = URL(string: url) else { return nil }
        var request = URLRequest(url: reqUrl)
        addToken(&request)
        guard let (data, response) = try? await session.data(for: request),
              (response as? HTTPURLResponse)?.statusCode == 200 else { return nil }
        return data
    }

    private func post(_ url: String, body: String) async -> Data? {
        guard let reqUrl = URL(string: url) else { return nil }
        var request = URLRequest(url: reqUrl)
        request.httpMethod = "POST"
        request.setValue("application/json", forHTTPHeaderField: "Content-Type")
        request.httpBody = body.data(using: .utf8)
        addToken(&request)
        guard let (data, response) = try? await session.data(for: request),
              let http = response as? HTTPURLResponse,
              http.statusCode >= 200 && http.statusCode < 300 else { return nil }
        return data
    }

    private func patch(_ url: String, body: String) async -> Data? {
        guard let reqUrl = URL(string: url) else { return nil }
        var request = URLRequest(url: reqUrl)
        request.httpMethod = "PATCH"
        request.setValue("application/json", forHTTPHeaderField: "Content-Type")
        request.httpBody = body.data(using: .utf8)
        addToken(&request)
        guard let (data, response) = try? await session.data(for: request),
              let http = response as? HTTPURLResponse,
              http.statusCode >= 200 && http.statusCode < 300 else { return nil }
        return data
    }

    @discardableResult
    private func put(_ url: String, body: String) async -> Data? {
        guard let reqUrl = URL(string: url) else { return nil }
        var request = URLRequest(url: reqUrl)
        request.httpMethod = "PUT"
        request.setValue("application/json", forHTTPHeaderField: "Content-Type")
        request.httpBody = body.data(using: .utf8)
        addToken(&request)
        guard let (data, response) = try? await session.data(for: request),
              let http = response as? HTTPURLResponse,
              http.statusCode >= 200 && http.statusCode < 300 else { return nil }
        return data
    }

    private func uploadMultipart(url: String, fileName: String, bytes: Data, mimeType: String) async throws -> Data {
        guard let reqUrl = URL(string: url) else { throw GiteaError.requestFailed }
        let boundary = "Boundary-\(UUID().uuidString)"
        var request = URLRequest(url: reqUrl)
        request.httpMethod = "POST"
        request.setValue("multipart/form-data; boundary=\(boundary)", forHTTPHeaderField: "Content-Type")
        addToken(&request)

        var body = Data()
        let boundaryLine = "--\(boundary)\r\n"
        body.append(boundaryLine.data(using: .utf8)!)
        body.append("Content-Disposition: form-data; name=\"attachment\"; filename=\"\(fileName)\"\r\n".data(using: .utf8)!)
        body.append("Content-Type: \(mimeType)\r\n\r\n".data(using: .utf8)!)
        body.append(bytes)
        body.append("\r\n--\(boundary)--\r\n".data(using: .utf8)!)
        request.httpBody = body

        let (data, response) = try await session.data(for: request)
        guard let http = response as? HTTPURLResponse, http.statusCode >= 200 && http.statusCode < 300 else {
            throw GiteaError.requestFailed
        }
        return data
    }

    private func addToken(_ request: inout URLRequest) {
        guard let token = keychain.get(CredentialKey.giteaToken) else { return }
        request.setValue("token \(token)", forHTTPHeaderField: "Authorization")
    }

    private func instanceUrl() -> String? {
        keychain.get(CredentialKey.giteaInstance).map {
            $0.trimmingCharacters(in: CharacterSet(charactersIn: "/"))
        }
    }

    private func jsonString(_ value: String) -> String {
        let escaped = value
            .replacingOccurrences(of: "\\", with: "\\\\")
            .replacingOccurrences(of: "\"", with: "\\\"")
            .replacingOccurrences(of: "\n", with: "\\n")
            .replacingOccurrences(of: "\r", with: "\\r")
            .replacingOccurrences(of: "\t", with: "\\t")
        return "\"\(escaped)\""
    }

    // ─── DTOs ─────────────────────────────────────────────────────────────────

    // Date parsing uses value-typed ISO8601FormatStyle (Sendable, macOS 12+).
    // Gitea uses RFC-3339 / ISO-8601, e.g. "2024-01-15T10:30:00Z" or
    // "2024-01-15T10:30:00.000Z". Try plain first, fall back to fractional seconds.
    private static func parseDate(_ string: String) -> Date {
        if let d = try? Date(string, strategy: .iso8601) { return d }
        if let d = try? Date(string, strategy: Date.ISO8601FormatStyle(includingFractionalSeconds: true)) { return d }
        return Date()
    }

    private struct GiteaIssueDto: Codable {
        let id: Int64
        let number: Int
        let title: String
        let body: String?
        let state: String
        let labels: [GiteaLabelDto]?
        let assignees: [GiteaUserDto]?
        let created_at: String
        let updated_at: String
        let html_url: String

        func toDomain(repo: String) -> GiteaIssue {
            GiteaIssue(
                id: id, number: number, title: title, body: body ?? "",
                state: state,
                labels: labels?.map(\.name) ?? [],
                assignees: assignees?.map(\.login) ?? [],
                createdAt: GiteaClient.parseDate(created_at),
                updatedAt: GiteaClient.parseDate(updated_at),
                repository: repo, htmlUrl: html_url
            )
        }
    }

    private struct GiteaCommentDto: Codable {
        let id: Int64
        let body: String
        let user: GiteaUserDto
        let created_at: String

        func toDomain() -> GiteaComment {
            GiteaComment(
                id: id, body: body, user: user.login,
                createdAt: GiteaClient.parseDate(created_at)
            )
        }
    }

    private struct GiteaLabelDto: Codable {
        let id: Int64
        let name: String
        let color: String
    }

    private struct GiteaUserDto: Codable {
        let login: String
    }

    fileprivate struct GiteaAttachmentDto: Codable {
        let id: Int64
        let name: String
        let browser_download_url: String
        let size: Int64
        let uuid: String

        func toDomain() -> GiteaAttachment {
            GiteaAttachment(id: id, name: name, downloadUrl: browser_download_url, size: size, uuid: uuid)
        }
    }
}

// ─── Errors ───────────────────────────────────────────────────────────────────

enum GiteaError: LocalizedError {
    case noInstance
    case requestFailed

    var errorDescription: String? {
        switch self {
        case .noInstance:    return "No Gitea instance configured."
        case .requestFailed: return "Gitea request failed."
        }
    }
}
