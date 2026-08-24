import Foundation
import CrossDashboardKit

struct KarakeepFolder: Codable, Identifiable, Hashable, Sendable {
    let id: String
    let name: String
    let parentId: String?
    let type: String?
    let userRole: String?
}

private struct KarakeepFolderResponse: Decodable {
    let lists: [KarakeepFolder]
}

private struct KarakeepBookmarkResponse: Decodable {
    let id: String
}

private struct CreateKarakeepBookmarkRequest: Encodable {
    let type = "link"
    let url: String
    let source = "api"
}

enum KarakeepError: LocalizedError {
    case missingServer
    case missingAPIKey
    case invalidServer
    case http(Int)
    case emptyResponse

    var errorDescription: String? {
        switch self {
        case .missingServer: "Karakeep server URL is missing"
        case .missingAPIKey: "Karakeep API key is missing"
        case .invalidServer: "Karakeep server URL is invalid"
        case .http(let status): "Karakeep returned HTTP \(status)"
        case .emptyResponse: "Karakeep returned an empty response"
        }
    }
}

final class KarakeepClient: Sendable {
    private let keychain: KeychainStore
    private let session: URLSession
    private let decoder = JSONDecoder()
    private let encoder = JSONEncoder()

    init(keychain: KeychainStore = .shared) {
        self.keychain = keychain
        let configuration = URLSessionConfiguration.default
        configuration.timeoutIntervalForRequest = 30
        configuration.timeoutIntervalForResource = 60
        session = URLSession(configuration: configuration)
    }

    func listFolders() async throws -> [KarakeepFolder] {
        let data = try await request(path: "lists")
        return try decoder.decode(KarakeepFolderResponse.self, from: data).lists
            .filter {
                ($0.type ?? "manual") == "manual" &&
                    ["owner", "editor"].contains($0.userRole ?? "owner")
            }
            .sorted { $0.name.localizedCaseInsensitiveCompare($1.name) == .orderedAscending }
    }

    func save(urls: [URL], folderId: String?) async throws {
        for url in urls {
            let payload = try encoder.encode(CreateKarakeepBookmarkRequest(url: url.absoluteString))
            let data = try await request(path: "bookmarks", method: "POST", body: payload)
            let bookmarkId = try decoder.decode(KarakeepBookmarkResponse.self, from: data).id
            if let folderId {
                _ = try await request(
                    path: "lists/\(folderId)/bookmarks/\(bookmarkId)",
                    method: "PUT",
                    body: Data(),
                    allowsEmptyResponse: true
                )
            }
        }
    }

    private func request(
        path: String,
        method: String = "GET",
        body: Data? = nil,
        allowsEmptyResponse: Bool = false
    ) async throws -> Data {
        guard let host = keychain.get(CredentialKey.karakeepHost)?.trimmingCharacters(in: CharacterSet(charactersIn: "/")),
              !host.isEmpty else { throw KarakeepError.missingServer }
        guard let token = keychain.get(CredentialKey.karakeepToken), !token.isEmpty else {
            throw KarakeepError.missingAPIKey
        }
        guard let url = URL(string: "\(host)/api/v1/\(path)") else { throw KarakeepError.invalidServer }
        var request = URLRequest(url: url)
        request.httpMethod = method
        request.httpBody = body
        request.setValue("application/json", forHTTPHeaderField: "Accept")
        request.setValue("Bearer \(token)", forHTTPHeaderField: "Authorization")
        if body != nil { request.setValue("application/json", forHTTPHeaderField: "Content-Type") }

        let (data, response) = try await session.data(for: request)
        guard let http = response as? HTTPURLResponse else { throw KarakeepError.emptyResponse }
        guard (200..<300).contains(http.statusCode) else { throw KarakeepError.http(http.statusCode) }
        if data.isEmpty && !allowsEmptyResponse { throw KarakeepError.emptyResponse }
        return data
    }
}
