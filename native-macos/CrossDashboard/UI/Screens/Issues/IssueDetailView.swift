import SwiftUI
import CrossDashboardKit

/// Issue detail panel with comments, attachments, and add-comment bar.
/// Mirrors IssuePropertySheet + IssueReadContent on Android.
struct IssueDetailView: View {

    @Environment(\.appContainer) private var container
    @Environment(PomodoroViewModel.self) private var pomodoroVM
    let issueID: Int64?
    @State private var viewModel = IssuesViewModel()
    @State private var showLabelEditor = false
    @State private var labelDraft = ""

    private var issue: GiteaIssue? {
        guard let id = issueID else { return nil }
        return container.issueRepository.allIssues.first { $0.id == id }
    }

    var body: some View {
        if let issue {
            VStack(spacing: 0) {
                issueContent(issue)
                Divider()
                AddCommentBar(
                    draft: Binding(get: { viewModel.commentDraft }, set: { viewModel.commentDraft = $0 }),
                    attachments: viewModel.pendingCommentAttachments,
                    isSubmitting: viewModel.isSubmittingComment,
                    onPickFile: { viewModel.pickAttachmentFile(for: .comment) },
                    onRemoveAttachment: { id in
                        viewModel.pendingCommentAttachments.removeAll { $0.id == id }
                    },
                    onSubmit: { viewModel.submitComment(for: issue) }
                )
            }
            .task(id: issueID) {
                await viewModel.loadComments(for: issue)
            }
            .toolbar {
                ToolbarItem(placement: .secondaryAction) {
                    Button {
                        labelDraft = issue.labels.joined(separator: ", ")
                        showLabelEditor = true
                    } label: {
                        Label("Edit Labels", systemImage: "tag")
                    }
                    .accessibilityLabel("Edit issue labels")
                }
                ToolbarItem(placement: .secondaryAction) {
                    Button {
                        pomodoroVM.startForIssue(title: issue.title)
                    } label: {
                        Label("Start Pomodoro", systemImage: "timer")
                    }
                    .accessibilityLabel("Start Pomodoro for this issue")
                }
                ToolbarItem(placement: .primaryAction) {
                    Button {
                        viewModel.openInBrowser(issue)
                    } label: {
                        Label("Open in Browser", systemImage: "safari")
                    }
                    .accessibilityLabel("Open issue in browser")
                }
                ToolbarItem {
                    Button {
                        viewModel.toggleIssueState(issue)
                    } label: {
                        Label(
                            issue.state == "open" ? "Close" : "Reopen",
                            systemImage: issue.state == "open" ? "checkmark.circle" : "arrow.uturn.backward.circle"
                        )
                    }
                    .accessibilityLabel(issue.state == "open" ? "Close issue" : "Reopen issue")
                }
            }
            .sheet(isPresented: $showLabelEditor) {
                NavigationStack {
                    Form {
                        TextField("Comma-separated labels", text: $labelDraft)
                        Text("New labels are created in Gitea. Existing Kanban and Covey labels are retained unless removed here.")
                            .font(.caption)
                            .foregroundStyle(.secondary)
                    }
                    .formStyle(.grouped)
                    .navigationTitle("Issue Labels")
                    .toolbar {
                        ToolbarItem(placement: .cancellationAction) {
                            Button("Cancel") { showLabelEditor = false }
                        }
                        ToolbarItem(placement: .confirmationAction) {
                            Button("Save") {
                                let labels = Array(Set(
                                    labelDraft.split(separator: ",")
                                        .map { $0.trimmingCharacters(in: .whitespacesAndNewlines) }
                                        .filter { !$0.isEmpty }
                                )).sorted { $0.localizedCaseInsensitiveCompare($1) == .orderedAscending }
                                viewModel.replaceLabels(for: issue, labels: labels)
                                showLabelEditor = false
                            }
                        }
                    }
                }
                .frame(minWidth: 460, minHeight: 220)
            }
            .alert("Error", isPresented: Binding(
                get: { viewModel.errorMessage != nil },
                set: { if !$0 { viewModel.errorMessage = nil } }
            )) {
                Button("OK") { viewModel.errorMessage = nil }
            } message: {
                Text(viewModel.errorMessage ?? "")
            }
        } else {
            ContentUnavailableView(
                "Select an issue",
                systemImage: "exclamationmark.bubble",
                description: Text("Choose an issue from the list to see its details.")
            )
        }
    }

    @ViewBuilder
    private func issueContent(_ issue: GiteaIssue) -> some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 14) {
                // Header
                VStack(alignment: .leading, spacing: 6) {
                    HStack(spacing: 8) {
                        Image(systemName: issue.state == "open" ? "circle" : "checkmark.circle.fill")
                            .foregroundStyle(issue.state == "open" ? .green : .purple)
                        Text("#\(issue.number)")
                            .foregroundStyle(.secondary)
                        Text(issue.repository)
                            .font(.caption)
                            .foregroundStyle(.secondary)
                    }

                    Text(issue.title)
                        .font(.title2)
                        .fontWeight(.semibold)

                    if let milestone = issue.milestoneTitle {
                        Label(milestone, systemImage: "flag")
                            .font(.caption)
                            .foregroundStyle(.secondary)
                    }

                    if !issue.labels.isEmpty {
                        ScrollView(.horizontal, showsIndicators: false) {
                            HStack(spacing: 6) {
                                ForEach(issue.labels, id: \.self) { TagChip(tag: $0) }
                            }
                        }
                    }

                    HStack {
                        if !issue.assignees.isEmpty {
                            Label(issue.assignees.joined(separator: ", "), systemImage: "person.2")
                                .font(.caption)
                                .foregroundStyle(.secondary)
                        }
                        Spacer()
                        Text("Updated \(issue.updatedAt, style: .relative)")
                            .font(.caption)
                            .foregroundStyle(.tertiary)
                    }
                }

                Divider()

                // Body
                if !issue.body.isEmpty {
                    ReadMarkdownView(content: issue.body)
                }

                // Issue attachments
                if !viewModel.issueAttachments.isEmpty {
                    attachmentsSection("Attachments", viewModel.issueAttachments)
                }

                Divider()

                // Comments
                if viewModel.isLoadingComments {
                    ProgressView("Loading comments…")
                        .frame(maxWidth: .infinity)
                } else if viewModel.comments.isEmpty {
                    Text("No comments yet.")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                        .frame(maxWidth: .infinity, alignment: .center)
                } else {
                    ForEach(viewModel.comments) { comment in
                        CommentRow(
                            comment: comment,
                            attachments: viewModel.commentAttachments[comment.id] ?? []
                        )
                    }
                }
            }
            .padding()
        }
    }

    @ViewBuilder
    private func attachmentsSection(_ title: String, _ attachments: [GiteaAttachment]) -> some View {
        VStack(alignment: .leading, spacing: 6) {
            Text(title)
                .font(.caption)
                .fontDesign(.monospaced)
                .textCase(.uppercase)
                .foregroundStyle(.secondary)

            ForEach(attachments) { att in
                AttachmentLink(attachment: att)
            }
        }
    }
}

// ─── CommentRow ───────────────────────────────────────────────────────────────

private struct CommentRow: View {
    let comment: GiteaComment
    let attachments: [GiteaAttachment]

    var body: some View {
        VStack(alignment: .leading, spacing: 6) {
            HStack {
                Label(comment.user, systemImage: "person.circle")
                    .font(.caption)
                    .fontWeight(.medium)
                Spacer()
                Text(comment.createdAt, style: .relative)
                    .font(.caption2)
                    .foregroundStyle(.tertiary)
            }

            ReadMarkdownView(content: comment.body)

            if !attachments.isEmpty {
                ForEach(attachments) { att in
                    AttachmentLink(attachment: att)
                }
            }
        }
        .padding(10)
        .background(RoundedRectangle(cornerRadius: 8).fill(Color(.windowBackgroundColor)))
        .overlay(
            RoundedRectangle(cornerRadius: 8)
                .strokeBorder(Color(.separatorColor), lineWidth: 0.5)
        )
        .accessibilityElement(children: .combine)
        .accessibilityLabel("Comment by \(comment.user)")
    }
}

// ─── AttachmentLink ───────────────────────────────────────────────────────────

struct AttachmentLink: View {
    let attachment: GiteaAttachment

    var body: some View {
        Button {
            if let url = URL(string: attachment.downloadUrl) {
                NSWorkspace.shared.open(url)
            }
        } label: {
            HStack(spacing: 6) {
                Image(systemName: "paperclip")
                    .accessibilityHidden(true)
                Text(attachment.name)
                    .lineLimit(1)
                Spacer()
                Text(fileSizeLabel)
                    .font(.caption2)
                    .foregroundStyle(.tertiary)
            }
            .font(.caption)
        }
        .buttonStyle(.plain)
        .accessibilityLabel("Attachment: \(attachment.name), \(fileSizeLabel)")
        .accessibilityHint("Opens in browser")
    }

    private var fileSizeLabel: String {
        let kb = Double(attachment.size) / 1024
        if kb < 1024 { return String(format: "%.0f KB", kb) }
        return String(format: "%.1f MB", kb / 1024)
    }
}

// ─── AddCommentBar ────────────────────────────────────────────────────────────

private struct AddCommentBar: View {
    @Binding var draft: String
    let attachments: [PendingAttachment]
    let isSubmitting: Bool
    let onPickFile: () -> Void
    let onRemoveAttachment: (UUID) -> Void
    let onSubmit: () -> Void

    @FocusState private var focused: Bool

    var body: some View {
        VStack(spacing: 0) {
            if !attachments.isEmpty {
                ScrollView(.horizontal, showsIndicators: false) {
                    HStack(spacing: 6) {
                        ForEach(attachments) { att in
                            HStack(spacing: 4) {
                                Text(att.fileName)
                                    .font(.caption)
                                    .lineLimit(1)
                                Button {
                                    onRemoveAttachment(att.id)
                                } label: {
                                    Image(systemName: "xmark.circle.fill")
                                        .font(.caption)
                                        .foregroundStyle(.secondary)
                                }
                                .buttonStyle(.plain)
                                .accessibilityLabel("Remove \(att.fileName)")
                            }
                            .padding(.horizontal, 8)
                            .padding(.vertical, 4)
                            .background(Capsule().fill(Color.accentColor.opacity(0.15)))
                        }
                    }
                    .padding(.horizontal, 12)
                    .padding(.vertical, 6)
                }
                Divider()
            }

            HStack(spacing: 8) {
                Button(action: onPickFile) {
                    Image(systemName: "paperclip")
                        .accessibilityLabel("Attach file")
                }
                .buttonStyle(.plain)
                .foregroundStyle(.secondary)

                TextField("Add a comment…", text: $draft, axis: .vertical)
                    .lineLimit(1...4)
                    .focused($focused)
                    .onSubmit {
                        if !draft.trimmingCharacters(in: .whitespaces).isEmpty {
                            onSubmit()
                        }
                    }

                if isSubmitting {
                    ProgressView().controlSize(.small)
                } else {
                    Button(action: onSubmit) {
                        Image(systemName: "arrow.up.circle.fill")
                            .font(.title3)
                            .foregroundStyle(draft.trimmingCharacters(in: .whitespaces).isEmpty ? AnyShapeStyle(.tertiary) : AnyShapeStyle(Color.accentColor))
                    }
                    .buttonStyle(.plain)
                    .disabled(draft.trimmingCharacters(in: .whitespaces).isEmpty)
                    .accessibilityLabel("Send comment")
                }
            }
            .padding(.horizontal, 12)
            .padding(.vertical, 8)
        }
        .background(.regularMaterial)
    }
}
