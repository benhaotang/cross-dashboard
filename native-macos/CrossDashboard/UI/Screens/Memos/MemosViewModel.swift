import Foundation
import Observation
import CrossDashboardKit

@Observable
@MainActor
final class MemosViewModel {

    // ─── Dependencies ─────────────────────────────────────────────────────────

    private let container: AppContainer

    var memoRepo: MemoRepository { container.memoRepository }

    // ─── Filter state ─────────────────────────────────────────────────────────

    var stateFilter: MemoState? = .normal    // nil = all
    var selectedTags: Set<String> = []
    var searchText: String = ""

    // ─── Memo data ────────────────────────────────────────────────────────────

    var isLoading: Bool = false
    var error: String? = nil
    var snackbarMessage: String? = nil

    /// Comments keyed by memo name — loaded lazily.
    var comments: [String: [MemosMemo]] = [:]
    var commentLoading: Set<String> = []

    var filteredMemos: [MemosMemo] {
        var all = memoRepo.allMemos
        if let state = stateFilter {
            all = all.filter { $0.state == state }
        }
        all = all.filter { memo in selectedTags.allSatisfy { memo.tags.contains($0) } }
        if !searchText.isEmpty {
            all = all.filter { $0.content.localizedCaseInsensitiveContains(searchText) || $0.snippet.localizedCaseInsensitiveContains(searchText) }
        }
        return all
    }

    var allTags: [String] {
        Array(Set(memoRepo.allMemos.flatMap { $0.tags })).sorted()
    }

    func clearFilters() {
        stateFilter = .normal
        selectedTags = []
    }

    var memosHost: String {
        container.keychain.get(CredentialKey.memosHost) ?? ""
    }

    var memosToken: String {
        container.keychain.get(CredentialKey.memosToken) ?? ""
    }

    var configuredRepos: [String] {
        guard let raw = container.keychain.get(CredentialKey.giteaRepos) else { return [] }
        // SettingsViewModel.saveGitea() encodes repos as a JSON array — decode that here.
        if let data = raw.data(using: .utf8),
           let arr = try? JSONDecoder().decode([String].self, from: data) {
            return arr.filter { !$0.isEmpty }
        }
        // Fallback: plain comma-separated (e.g. Android-side format or manual entry)
        return raw.split(separator: ",").map { $0.trimmingCharacters(in: .whitespaces) }.filter { !$0.isEmpty }
    }

    /// Open issues cached locally for a given repo, used by CommentOnIssueSheet.
    func issues(for repo: String) -> [GiteaIssue] {
        container.issueRepository.allIssues.filter { $0.repository == repo && $0.state == "open" }
    }

    // ─── Init ─────────────────────────────────────────────────────────────────

    init(container: AppContainer = .shared) {
        self.container = container
    }

    // ─── Sync ─────────────────────────────────────────────────────────────────

    func sync() async {
        isLoading = true
        defer { isLoading = false }
        await memoRepo.syncMemos()
    }

    // ─── Create ───────────────────────────────────────────────────────────────

    func createMemo(content: String, visibility: MemoVisibility, attachments: [MemoPendingAttachment]) async {
        guard !content.isEmpty else { return }
        isLoading = true
        defer { isLoading = false }
        if await memoRepo.createMemo(content: content, visibility: visibility, attachments: attachments) != nil {
            snackbarMessage = "Memo created"
        } else {
            error = "Failed to create memo"
        }
    }

    // ─── Delete / Archive ─────────────────────────────────────────────────────

    func deleteMemo(_ name: String, force: Bool = false) async {
        await memoRepo.deleteMemo(name, force: force)
        snackbarMessage = "Memo deleted"
    }

    func archiveMemo(_ name: String) async {
        await memoRepo.archiveMemo(name)
        snackbarMessage = "Memo archived"
    }

    func restoreMemo(_ name: String) async {
        await memoRepo.restoreMemo(name)
        snackbarMessage = "Memo restored"
    }

    // ─── Comments ─────────────────────────────────────────────────────────────

    func loadComments(for memoName: String) async {
        guard !commentLoading.contains(memoName) else { return }
        commentLoading.insert(memoName)
        defer { commentLoading.remove(memoName) }
        let result = await memoRepo.loadComments(memoName)
        comments[memoName] = result
    }

    func addComment(to memoName: String, content: String) async {
        guard !content.isEmpty else { return }
        _ = await memoRepo.createComment(parentId: memoName, content: content)
        await loadComments(for: memoName)
    }

    // ─── Share ────────────────────────────────────────────────────────────────

    func createShare(_ memoId: String) async -> String? {
        await memoRepo.createShare(memoId)
    }

    // ─── Action helpers ───────────────────────────────────────────────────────

    func extractTasks(from memo: MemosMemo) -> [ParsedTask] {
        return memo.content.components(separatedBy: "\n")
            .filter { $0.trimmingCharacters(in: .whitespaces).hasPrefix("- [ ]") }
            .compactMap { line -> ParsedTask? in
                let raw = line.trimmingCharacters(in: .whitespaces)
                    .replacingOccurrences(of: "- [ ]", with: "")
                    .trimmingCharacters(in: .whitespaces)
                guard !raw.isEmpty else { return nil }
                return TaskInputParser.parse(input: raw)
            }
    }

    func detectFirstDate(in memo: MemosMemo) -> Date? {
        let lower = memo.content.lowercased()
        if lower.contains("today")    { return Date() }
        if lower.contains("tomorrow") { return Calendar.current.date(byAdding: .day, value: 1, to: Date()) }
        // ISO date
        let regex = try? NSRegularExpression(pattern: #"\d{4}-\d{2}-\d{2}"#)
        if let match = regex?.firstMatch(in: memo.content, range: NSRange(memo.content.startIndex..., in: memo.content)),
           let range = Range(match.range, in: memo.content) {
            let dateStr = String(memo.content[range])
            let fmt = DateFormatter(); fmt.dateFormat = "yyyy-MM-dd"
            return fmt.date(from: dateStr)
        }
        return nil
    }

    func firstURL(in memo: MemosMemo) -> URL? {
        let detector = try? NSDataDetector(types: NSTextCheckingResult.CheckingType.link.rawValue)
        let range = NSRange(memo.content.startIndex..., in: memo.content)
        return detector?.firstMatch(in: memo.content, range: range)
            .flatMap { Range($0.range, in: memo.content) }
            .flatMap { URL(string: String(memo.content[$0])) }
    }

    func memoURL(for memo: MemosMemo) -> URL? {
        guard !memosHost.isEmpty else { return nil }
        return URL(string: "\(memosHost)/\(memo.name)")
    }

    // ─── Task / Event / Issue helpers ─────────────────────────────────────────

    func createTaskFromParsed(_ parsed: ParsedTask, calendarHref: String) async {
        let task = CalDavTask(summary: parsed.summary, priority: parsed.priority, due: parsed.due, categories: parsed.categories)
        _ = try? await container.taskRepository.create(task, calendarHref: calendarHref)
    }

    func createEventFromMemo(summary: String, description: String, start: Date, end: Date, calendarHref: String) async {
        let event = CalendarEvent(uid: UUID().uuidString, summary: summary, start: start, end: end, description: description)
        _ = try? await container.eventRepository.create(event, calendarHref: calendarHref)
        snackbarMessage = "Event created"
    }

    func addCommentToIssue(repo: String, issueNumber: Int, body: String) async {
        _ = try? await container.issueRepository.addComment(repo: repo, number: issueNumber, body: body)
        snackbarMessage = "Comment added"
    }

    func clearSnackbar() { snackbarMessage = nil }
    func clearError() { error = nil }
}
