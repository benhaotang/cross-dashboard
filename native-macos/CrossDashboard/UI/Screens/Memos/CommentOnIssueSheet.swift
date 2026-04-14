import SwiftUI
import CrossDashboardKit

struct CommentOnIssueSheetMac: View {

    let memo: MemosMemo
    var viewModel: MemosViewModel
    @Environment(\.dismiss) private var dismiss

    @State private var selectedRepo: String = ""
    @State private var selectedIssue: GiteaIssue? = nil
    @State private var commentBody: String = ""

    private var repoIssues: [GiteaIssue] {
        viewModel.issues(for: selectedRepo)
    }

    var body: some View {
        Form {
            Section("Issue") {
                Picker("Repository", selection: $selectedRepo) {
                    ForEach(viewModel.configuredRepos, id: \.self) { repo in
                        Text(repo).tag(repo)
                    }
                }
                .onChange(of: selectedRepo) { _, _ in selectedIssue = nil }

                if repoIssues.isEmpty {
                    Text(selectedRepo.isEmpty
                         ? "No repository configured — add one in Settings → Gitea."
                         : "No open issues synced for this repository.")
                        .foregroundStyle(.secondary)
                        .font(.caption)
                } else {
                    Picker("Issue", selection: $selectedIssue) {
                        Text("Select an issue…").tag(Optional<GiteaIssue>.none)
                        ForEach(repoIssues) { issue in
                            Text("#\(issue.number)  \(issue.title)").tag(Optional(issue))
                        }
                    }
                }
            }

            Section("Comment") {
                TextEditor(text: $commentBody)
                    .frame(minHeight: 80)
            }

            Section {
                HStack {
                    Spacer()
                    Button("Cancel", role: .cancel) { dismiss() }
                    Button("Post Comment") {
                        guard let issue = selectedIssue else { return }
                        Task {
                            await viewModel.addCommentToIssue(
                                repo: selectedRepo,
                                issueNumber: issue.number,
                                body: commentBody
                            )
                            dismiss()
                        }
                    }
                    .buttonStyle(.borderedProminent)
                    .disabled(selectedRepo.isEmpty || selectedIssue == nil || commentBody.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty)
                }
            }
        }
        .formStyle(.grouped)
        .padding()
        .frame(minWidth: 460, minHeight: 340)
        .onAppear {
            selectedRepo = viewModel.configuredRepos.first ?? ""
            let host = viewModel.memosHost
            let memoUrl = host.isEmpty ? "" : "\(host)/\(memo.name)"
            commentBody = memoUrl.isEmpty ? memo.snippet : "\(memo.snippet)\n\n\(memoUrl)"
        }
    }
}
