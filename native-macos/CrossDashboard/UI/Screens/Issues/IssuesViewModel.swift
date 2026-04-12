import Foundation
import Observation
import AppKit
import CrossDashboardKit

/// UI-layer pending attachment (mirrors PendingAttachment on Android).
struct PendingAttachment: Identifiable, Sendable {
    let id: UUID = UUID()
    let fileName: String
    let mimeType: String
    let data: Data
}

@Observable
@MainActor
final class IssuesViewModel {

    // ─── Filter ───────────────────────────────────────────────────────────────

    enum Filter: String, CaseIterable, Identifiable {
        case open   = "Open"
        case closed = "Closed"
        case all    = "All"
        var id: String { rawValue }
    }

    var filter: Filter = .open
    var searchText: String = ""
    var selectedIssueID: Int64?
    var isLoading: Bool = false
    var errorMessage: String?

    // ─── Comments / detail ────────────────────────────────────────────────────

    var comments: [GiteaComment] = []
    var issueAttachments: [GiteaAttachment] = []
    var commentAttachments: [Int64: [GiteaAttachment]] = [:]
    var isLoadingComments: Bool = false

    // ─── Add comment ──────────────────────────────────────────────────────────

    var commentDraft: String = ""
    var pendingCommentAttachments: [PendingAttachment] = []
    var isSubmittingComment: Bool = false

    // ─── Create issue sheet ───────────────────────────────────────────────────

    var showCreateSheet: Bool = false
    var createTitle: String = ""
    var createBody: String = ""
    var createRepo: String = ""
    var createAttachments: [PendingAttachment] = []

    // ─── Dependencies ─────────────────────────────────────────────────────────

    private let container: AppContainer

    init(container: AppContainer = .shared) {
        self.container = container
        createRepo = giteaRepositories().first ?? ""
    }

    // ─── Derived data ─────────────────────────────────────────────────────────

    var allIssues: [GiteaIssue] { container.issueRepository.allIssues }

    var filteredIssues: [GiteaIssue] {
        let base: [GiteaIssue]
        switch filter {
        case .open:   base = allIssues.filter { $0.state == "open" }
        case .closed: base = allIssues.filter { $0.state == "closed" }
        case .all:    base = allIssues
        }
        guard !searchText.isEmpty else { return base }
        let q = searchText.lowercased()
        return base.filter { $0.title.lowercased().contains(q) || $0.body.lowercased().contains(q) }
    }

    var selectedIssue: GiteaIssue? {
        guard let id = selectedIssueID else { return nil }
        return allIssues.first { $0.id == id }
    }

    var giteaRepos: [String] { giteaRepositories() }

    // ─── Actions ──────────────────────────────────────────────────────────────

    func sync() async {
        isLoading = true
        defer { isLoading = false }
        await container.syncAll()
    }

    func loadComments(for issue: GiteaIssue) async {
        isLoadingComments = true
        defer { isLoadingComments = false }
        let (repo, number) = (issue.repository, issue.number)

        async let commentsFetch     = container.issueRepository.fetchComments(repo: repo, number: number)
        async let issueAttachFetch  = container.issueRepository.fetchIssueAttachments(repo: repo, issueNumber: number)
        let (fetchedComments, issueAttach) = await (commentsFetch, issueAttachFetch)

        self.comments = fetchedComments
        self.issueAttachments = issueAttach

        var commentAttachMap: [Int64: [GiteaAttachment]] = [:]
        await withTaskGroup(of: (Int64, [GiteaAttachment]).self) { group in
            for comment in fetchedComments {
                group.addTask {
                    let attachments = await self.container.issueRepository.fetchCommentAttachments(
                        repo: repo, commentId: comment.id
                    )
                    return (comment.id, attachments)
                }
            }
            for await (cid, attachments) in group {
                commentAttachMap[cid] = attachments
            }
        }
        self.commentAttachments = commentAttachMap
    }

    func toggleIssueState(_ issue: GiteaIssue) {
        let newState = issue.state == "open" ? "closed" : "open"
        Task {
            do {
                _ = try await container.issueRepository.update(
                    repo: issue.repository,
                    number: issue.number,
                    state: newState
                )
            } catch {
                errorMessage = error.localizedDescription
            }
        }
    }

    func submitComment(for issue: GiteaIssue) {
        guard !commentDraft.trimmingCharacters(in: .whitespaces).isEmpty else { return }
        let body = commentDraft
        let attachments = pendingCommentAttachments
        commentDraft = ""
        pendingCommentAttachments = []
        isSubmittingComment = true

        Task {
            defer { isSubmittingComment = false }
            do {
                let comment = try await container.issueRepository.addComment(
                    repo: issue.repository, number: issue.number, body: body
                )
                for att in attachments {
                    _ = try? await container.issueRepository.attachToComment(
                        repo: issue.repository,
                        commentId: comment.id,
                        fileName: att.fileName,
                        bytes: att.data,
                        mimeType: att.mimeType
                    )
                }
                await loadComments(for: issue)
            } catch {
                errorMessage = error.localizedDescription
            }
        }
    }

    func openInBrowser(_ issue: GiteaIssue) {
        if let url = URL(string: issue.htmlUrl) {
            NSWorkspace.shared.open(url)
        }
    }

    func createIssue() {
        guard !createTitle.trimmingCharacters(in: .whitespaces).isEmpty else { return }
        let title = createTitle
        let body = createBody
        let repo = createRepo
        let attachments = createAttachments
        showCreateSheet = false
        createTitle = ""
        createBody = ""
        createAttachments = []

        Task {
            do {
                let issue = try await container.issueRepository.createIssue(
                    repo: repo, title: title, body: body
                )
                for att in attachments {
                    _ = try? await container.issueRepository.attachToIssue(
                        repo: repo,
                        issueNumber: issue.number,
                        fileName: att.fileName,
                        bytes: att.data,
                        mimeType: att.mimeType
                    )
                }
                selectedIssueID = issue.id
            } catch {
                errorMessage = error.localizedDescription
            }
        }
    }

    func pickAttachmentFile(for purpose: AttachmentPurpose) {
        let panel = NSOpenPanel()
        panel.canChooseFiles = true
        panel.canChooseDirectories = false
        panel.allowsMultipleSelection = true
        panel.begin { response in
            guard response == .OK else { return }
            for url in panel.urls {
                guard let data = try? Data(contentsOf: url) else { continue }
                let att = PendingAttachment(
                    fileName: url.lastPathComponent,
                    mimeType: url.mimeTypeHint,
                    data: data
                )
                Task { @MainActor in
                    switch purpose {
                    case .comment: self.pendingCommentAttachments.append(att)
                    case .create:  self.createAttachments.append(att)
                    }
                }
            }
        }
    }

    enum AttachmentPurpose { case comment, create }

    // ─── Helpers ──────────────────────────────────────────────────────────────

    private func giteaRepositories() -> [String] {
        guard let raw = container.keychain.get(CredentialKey.giteaRepos) else { return [] }
        return (try? JSONDecoder().decode([String].self, from: Data(raw.utf8))) ?? []
    }
}

// ─── URL MIME hint ────────────────────────────────────────────────────────────

private extension URL {
    var mimeTypeHint: String {
        switch pathExtension.lowercased() {
        case "png":  return "image/png"
        case "jpg", "jpeg": return "image/jpeg"
        case "gif":  return "image/gif"
        case "pdf":  return "application/pdf"
        case "txt":  return "text/plain"
        case "md":   return "text/markdown"
        case "zip":  return "application/zip"
        default:     return "application/octet-stream"
        }
    }
}
