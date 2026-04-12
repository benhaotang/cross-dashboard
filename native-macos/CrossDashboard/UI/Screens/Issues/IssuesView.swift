import SwiftUI
import CrossDashboardKit

/// Issue list with state filter, create FAB, and comment support.
/// Mirrors IssuesScreen on Android.
struct IssuesView: View {

    @Environment(AppViewModel.self) private var appViewModel
    @State private var viewModel = IssuesViewModel()

    var body: some View {
        @Bindable var vm = viewModel
        Group {
            if viewModel.isLoading && viewModel.allIssues.isEmpty {
                ProgressView("Loading issues…")
                    .frame(maxWidth: .infinity, maxHeight: .infinity)
            } else if viewModel.filteredIssues.isEmpty {
                ContentUnavailableView(
                    viewModel.searchText.isEmpty ? "No issues" : "No results",
                    systemImage: "exclamationmark.bubble",
                    description: Text(
                        viewModel.searchText.isEmpty
                            ? "No \(viewModel.filter.rawValue.lowercased()) issues."
                            : "No issues match '\(viewModel.searchText)'."
                    )
                )
            } else {
                List(viewModel.filteredIssues, selection: $vm.selectedIssueID) { issue in
                    IssueRow(issue: issue)
                        .tag(issue.id)
                        .contextMenu {
                            Button {
                                viewModel.toggleIssueState(issue)
                            } label: {
                                Label(
                                    issue.state == "open" ? "Close issue" : "Reopen issue",
                                    systemImage: issue.state == "open" ? "checkmark.circle" : "arrow.uturn.backward.circle"
                                )
                            }
                            Button {
                                viewModel.openInBrowser(issue)
                            } label: {
                                Label("Open in Browser", systemImage: "safari")
                            }
                        }
                }
                .listStyle(.inset)
            }
        }
        .navigationTitle("Issues")
        .searchable(text: $vm.searchText, prompt: "Search issues")
        .toolbar { toolbarContent }
        .sheet(isPresented: $vm.showCreateSheet) {
            CreateIssueSheet(viewModel: viewModel)
                .interactiveDismissDisabled(
                    !viewModel.createTitle.isEmpty ||
                    !viewModel.createBody.isEmpty ||
                    !viewModel.createAttachments.isEmpty
                )
        }
        .onChange(of: viewModel.selectedIssueID) { _, id in
            appViewModel.selectedIssueID = id
        }
        .alert("Error", isPresented: Binding(
            get: { viewModel.errorMessage != nil },
            set: { if !$0 { viewModel.errorMessage = nil } }
        )) {
            Button("OK") { viewModel.errorMessage = nil }
        } message: {
            Text(viewModel.errorMessage ?? "")
        }
    }

    @ToolbarContentBuilder
    private var toolbarContent: some ToolbarContent {
        ToolbarItem(placement: .primaryAction) {
            Button {
                viewModel.showCreateSheet = true
            } label: {
                Label("New Issue", systemImage: "plus")
            }
            .accessibilityLabel("Create issue")
        }
        ToolbarItem {
            Picker("State", selection: Binding(
                get: { viewModel.filter },
                set: { viewModel.filter = $0 }
            )) {
                ForEach(IssuesViewModel.Filter.allCases) { f in
                    Text(f.rawValue).tag(f)
                }
            }
            .pickerStyle(.segmented)
            .accessibilityLabel("Filter by state")
        }
        ToolbarItem {
            Button {
                Task { await viewModel.sync() }
            } label: {
                Label("Sync", systemImage: "arrow.clockwise")
            }
            .accessibilityLabel("Sync issues")
        }
    }
}

// ─── IssueRow ─────────────────────────────────────────────────────────────────

private struct IssueRow: View {
    let issue: GiteaIssue

    var body: some View {
        HStack(spacing: 10) {
            Image(systemName: issue.state == "open" ? "circle" : "checkmark.circle.fill")
                .foregroundStyle(issue.state == "open" ? Color.green : Color.purple)
                .accessibilityHidden(true)

            VStack(alignment: .leading, spacing: 2) {
                Text(issue.title)
                    .fontWeight(.medium)
                    .lineLimit(1)

                HStack(spacing: 6) {
                    Text(issue.repository)
                        .font(.caption)
                        .foregroundStyle(.secondary)
                    Text("#\(issue.number)")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                    if !issue.labels.isEmpty {
                        ForEach(issue.labels.prefix(2), id: \.self) { label in
                            TagChip(tag: label)
                        }
                    }
                }
            }

            Spacer()

            Text(issue.updatedAt, style: .relative)
                .font(.caption2)
                .foregroundStyle(.tertiary)
        }
        .padding(.vertical, 4)
        .accessibilityElement(children: .combine)
        .accessibilityLabel("\(issue.state == "open" ? "Open" : "Closed") issue: \(issue.title)")
        .accessibilityValue("\(issue.repository) #\(issue.number), updated \(issue.updatedAt.formatted(.relative(presentation: .named)))")
        .accessibilityHint("Activate to view issue details")
    }
}

// ─── CreateIssueSheet ─────────────────────────────────────────────────────────

struct CreateIssueSheet: View {
    @Bindable var viewModel: IssuesViewModel

    var body: some View {
        NavigationStack {
            Form {
                Section("Issue") {
                    if viewModel.giteaRepos.count > 1 {
                        Picker("Repository", selection: $viewModel.createRepo) {
                            ForEach(viewModel.giteaRepos, id: \.self) { repo in
                                Text(repo).tag(repo)
                            }
                        }
                    }
                    TextField("Title", text: $viewModel.createTitle)
                    TextEditor(text: $viewModel.createBody)
                        .frame(minHeight: 100)
                }

                Section("Attachments") {
                    if !viewModel.createAttachments.isEmpty {
                        ForEach(viewModel.createAttachments) { att in
                            Label(att.fileName, systemImage: "paperclip")
                                .font(.caption)
                        }
                    }
                    Button {
                        viewModel.pickAttachmentFile(for: .create)
                    } label: {
                        Label("Attach file…", systemImage: "plus")
                    }
                }
            }
            .formStyle(.grouped)
            .navigationTitle("New Issue")
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button("Cancel") { viewModel.showCreateSheet = false }
                }
                ToolbarItem(placement: .confirmationAction) {
                    Button("Create") { viewModel.createIssue() }
                        .disabled(viewModel.createTitle.trimmingCharacters(in: .whitespaces).isEmpty)
                }
            }
        }
        .frame(minWidth: 480, minHeight: 360)
    }
}
