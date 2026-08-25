import Foundation
import SwiftData
import CrossDashboardKit

/// Mirrors IssueRepository.kt.
@Observable
@MainActor
final class IssueRepository {

    private let context: ModelContext
    private let client: GiteaClient
    private let statsRepo: StatsRepository

    var allIssues: [GiteaIssue] = []

    var openIssues: [GiteaIssue] { allIssues.filter { $0.state == "open" } }

    init(context: ModelContext, client: GiteaClient, statsRepo: StatsRepository) {
        self.context   = context
        self.client    = client
        self.statsRepo = statsRepo
        loadFromDB()
    }

    func loadFromDB() {
        var descriptor = FetchDescriptor<IssueModel>()
        descriptor.includePendingChanges = false
        let models = (try? context.fetch(descriptor)) ?? []
        allIssues = models.map { $0.toDomain() }.sorted { $0.updatedAt > $1.updatedAt }
    }

    @discardableResult
    func sync(repositories: [String]) async -> Bool {
        guard !repositories.isEmpty else { return true }
        async let openFetch = client.fetchIssuesForSync(repositories: repositories, state: "open")
        async let closedFetch = client.fetchIssuesForSync(repositories: repositories, state: "closed")
        let (open, closed) = await (openFetch, closedFetch)
        guard let open, let closed else { return false }
        let all = open + closed

        do {
            try context.transaction {
                try context.delete(model: IssueModel.self)
                all.forEach { context.insert(IssueModel(from: $0)) }
            }
        } catch {
            context.rollback()
            print("[IssueRepository] Failed to replace issues: \(error)")
            return false
        }
        loadFromDB()
        return true
    }

    func update(
        repo: String,
        number: Int,
        title: String? = nil,
        body: String? = nil,
        state: String? = nil
    ) async throws -> GiteaIssue {
        let updated = try await client.updateIssue(repo: repo, number: number, title: title, body: body, state: state)
        upsertInDB(updated)
        if state == "closed" {
            statsRepo.incrementIssuesClosed()
        }
        return updated
    }

    func fetchComments(repo: String, number: Int) async -> [GiteaComment] {
        await client.fetchComments(repo: repo, number: number)
    }

    func addComment(repo: String, number: Int, body: String) async throws -> GiteaComment {
        try await client.addComment(repo: repo, number: number, body: body)
    }

    func replaceLabels(repo: String, number: Int, labelNames: [String]) async throws {
        let existing = await client.fetchLabels(repo: repo)
        var ids: [Int64] = []
        for name in labelNames {
            if let found = existing.first(where: { $0.name == name }) {
                ids.append(found.id)
            } else {
                let created = try await client.createRepoLabel(repo: repo, name: name)
                ids.append(created.id)
            }
        }
        try await client.replaceIssueLabels(repo: repo, number: number, labelIds: ids)
        // Reload this issue from server
        async let openFetch   = client.fetchIssues(repositories: [repo], state: "open")
        async let closedFetch = client.fetchIssues(repositories: [repo], state: "closed")
        let (open, closed) = await (openFetch, closedFetch)
        if let updated = (open + closed).first(where: { $0.number == number }) {
            upsertInDB(updated)
        }
    }

    func createIssue(repo: String, title: String, body: String) async throws -> GiteaIssue {
        let issue = try await client.createIssue(repo: repo, title: title, body: body)
        context.insert(IssueModel(from: issue))
        try? context.save()
        loadFromDB()
        return issue
    }

    func attachToIssue(
        repo: String, issueNumber: Int,
        fileName: String, bytes: Data, mimeType: String
    ) async throws -> String {
        try await client.uploadIssueAttachment(repo: repo, issueNumber: issueNumber, fileName: fileName, bytes: bytes, mimeType: mimeType)
    }

    func attachToComment(
        repo: String, commentId: Int64,
        fileName: String, bytes: Data, mimeType: String
    ) async throws -> String {
        try await client.uploadCommentAttachment(repo: repo, commentId: commentId, fileName: fileName, bytes: bytes, mimeType: mimeType)
    }

    func fetchIssueAttachments(repo: String, issueNumber: Int) async -> [GiteaAttachment] {
        await client.fetchIssueAttachments(repo: repo, issueNumber: issueNumber)
    }

    func fetchCommentAttachments(repo: String, commentId: Int64) async -> [GiteaAttachment] {
        await client.fetchCommentAttachments(repo: repo, commentId: commentId)
    }

    private func upsertInDB(_ issue: GiteaIssue) {
        let models = (try? context.fetch(FetchDescriptor<IssueModel>())) ?? []
        if let existing = models.first(where: { $0.issueId == issue.id }) {
            context.delete(existing)
        }
        context.insert(IssueModel(from: issue))
        try? context.save()
        loadFromDB()
    }
}
