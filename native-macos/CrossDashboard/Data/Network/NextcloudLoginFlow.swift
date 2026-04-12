import Foundation
import CrossDashboardKit

/// Nextcloud Login Flow v2 (browser-based).
///
/// Flow:
/// 1. POST to /index.php/login/v2 → get loginUrl + pollToken + pollEndpoint
/// 2. Open loginUrl in system browser (NSWorkspace.open) — user approves
/// 3. Poll pollEndpoint with pollToken every 2s for up to 5 min
/// 4. Returns (serverUrl, loginName, appPassword) to store in KeychainStore
///
/// Direct Swift port of NextcloudLoginFlow.kt.
final class NextcloudLoginFlow: Sendable {

    private let session: URLSession

    init() {
        let config = URLSessionConfiguration.default
        config.timeoutIntervalForRequest = 15
        self.session = URLSession(configuration: config)
    }

    // ─── Data types ───────────────────────────────────────────────────────────

    struct FlowInit: Sendable {
        let loginUrl: String
        let pollEndpoint: String
        let pollToken: String
    }

    struct LoginCredentials: Sendable {
        let serverUrl: String
        let loginName: String
        let appPassword: String
    }

    // ─── Step 1: initiate ─────────────────────────────────────────────────────

    func initiate(serverUrl: String) async -> Result<FlowInit, Error> {
        let urlStr = "\(serverUrl.trimmingCharacters(in: CharacterSet(charactersIn: "/")))/index.php/login/v2"
        guard let url = URL(string: urlStr) else {
            return .failure(LoginFlowError.invalidUrl(urlStr))
        }
        var request = URLRequest(url: url)
        request.httpMethod = "POST"
        request.setValue("application/x-www-form-urlencoded", forHTTPHeaderField: "Content-Type")
        request.httpBody = Data()

        do {
            let (data, response) = try await session.data(for: request)
            guard let http = response as? HTTPURLResponse, http.statusCode == 200 else {
                return .failure(LoginFlowError.httpError((response as? HTTPURLResponse)?.statusCode ?? 0))
            }
            let dto = try JSONDecoder().decode(LoginFlowInitDto.self, from: data)
            return .success(FlowInit(
                loginUrl:     dto.login,
                pollEndpoint: dto.poll.endpoint,
                pollToken:    dto.poll.token
            ))
        } catch {
            return .failure(error)
        }
    }

    // ─── Step 3: poll ─────────────────────────────────────────────────────────

    func poll(
        pollEndpoint: String,
        pollToken: String,
        timeoutSeconds: TimeInterval = 5 * 60
    ) async -> Result<LoginCredentials, Error> {
        guard let url = URL(string: pollEndpoint) else {
            return .failure(LoginFlowError.invalidUrl(pollEndpoint))
        }
        let deadline = Date().addingTimeInterval(timeoutSeconds)

        while Date() < deadline {
            try? await Task.sleep(for: .seconds(2))

            var request = URLRequest(url: url)
            request.httpMethod = "POST"
            request.setValue("application/x-www-form-urlencoded", forHTTPHeaderField: "Content-Type")
            request.httpBody = "token=\(pollToken)".data(using: .utf8)

            do {
                let (data, response) = try await session.data(for: request)
                if let http = response as? HTTPURLResponse, http.statusCode == 200 {
                    let dto = try JSONDecoder().decode(LoginCredentialsDto.self, from: data)
                    return .success(LoginCredentials(
                        serverUrl:   dto.server,
                        loginName:   dto.loginName,
                        appPassword: dto.appPassword
                    ))
                }
                // 404 = not yet approved; keep polling
            } catch {
                return .failure(error)
            }
        }
        return .failure(LoginFlowError.timeout)
    }

    // ─── Private DTOs ─────────────────────────────────────────────────────────

    private struct LoginFlowInitDto: Decodable {
        let poll: PollDto
        let login: String
    }

    private struct PollDto: Decodable {
        let token: String
        let endpoint: String
    }

    private struct LoginCredentialsDto: Decodable {
        let server: String
        let loginName: String
        let appPassword: String
    }
}

// ─── Errors ───────────────────────────────────────────────────────────────────

enum LoginFlowError: LocalizedError {
    case invalidUrl(String)
    case httpError(Int)
    case timeout

    var errorDescription: String? {
        switch self {
        case .invalidUrl(let u): return "Invalid URL: \(u)"
        case .httpError(let c):  return "HTTP \(c)"
        case .timeout:           return "Login flow timed out after 5 minutes."
        }
    }
}
