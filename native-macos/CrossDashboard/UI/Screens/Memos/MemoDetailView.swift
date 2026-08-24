import SwiftUI
import MarkdownUI
import CrossDashboardKit
import AppKit

/// Detail column for the Memos screen.
struct MemoDetailView: View {

    let memoName: String?

    @Environment(AppViewModel.self) private var appViewModel
    @State private var viewModel = MemosViewModel()
    @State private var commentInput = ""
    @State private var showExtractSheet = false
    @State private var showEventSheet = false
    @State private var showCommentIssueSheet = false
    @State private var showKarakeepSheet = false

    private var memo: MemosMemo? {
        viewModel.memoRepo.allMemos.first { $0.name == memoName }
    }

    var body: some View {
        Group {
            if let memo {
                detail(memo: memo)
            } else {
                ContentUnavailableView(
                    "Select a Memo",
                    systemImage: "tray.and.arrow.down",
                    description: Text("Choose a memo from the list.")
                )
            }
        }
    }

    @ViewBuilder
    private func detail(memo: MemosMemo) -> some View {
        let detectedURLs = viewModel.urls(in: memo)
        let dateStr: String = {
            let fmt = DateFormatter(); fmt.dateStyle = .medium; fmt.timeStyle = .short
            return fmt.string(from: memo.displayTime)
        }()

        ScrollView {
            LazyVStack(alignment: .leading, spacing: 0) {
                // ── Header ────────────────────────────────────────────────────
                VStack(alignment: .leading, spacing: 6) {
                    if !memo.property.title.isEmpty {
                        Text(memo.property.title).font(.title2).fontWeight(.semibold)
                    }
                    HStack(spacing: 8) {
                        Text(dateStr).font(.caption).foregroundStyle(.secondary)
                        Text("·").foregroundStyle(.secondary)
                        Text(memo.visibility.rawValue.lowercased()).font(.caption).foregroundStyle(.secondary)
                    }
                    if !memo.tags.isEmpty {
                        HStack(spacing: 4) {
                            ForEach(memo.tags, id: \.self) { tag in
                                Text("#\(tag)").font(.caption).foregroundStyle(.tint)
                            }
                        }
                    }
                }
                .padding([.horizontal, .top], 20)
                .padding(.bottom, 8)

                Divider()

                // ── Body ──────────────────────────────────────────────────────
                Markdown(memo.content)
                    .markdownTheme(.gitHub)
                    .padding(.horizontal, 20)
                    .padding(.vertical, 12)

                Divider()

                // ── Attachments ───────────────────────────────────────────────
                if !memo.attachments.isEmpty {
                    VStack(alignment: .leading, spacing: 6) {
                        Text("Attachments").font(.headline).padding(.horizontal, 20)
                        ForEach(memo.attachments) { att in
                            let fileUrl: String = att.externalLink.isEmpty
                                ? "\(viewModel.memosHost)/file/\(att.name)/\(att.filename)"
                                : att.externalLink
                            let isImage = att.type.hasPrefix("image/") &&
                                (att.type.contains("jpeg") || att.type.contains("png") ||
                                 att.type.contains("gif")  || att.type.contains("webp")) ||
                                att.filename.lowercased().hasSuffix(".jpg")  ||
                                att.filename.lowercased().hasSuffix(".jpeg") ||
                                att.filename.lowercased().hasSuffix(".png")

                            VStack(alignment: .leading, spacing: 4) {
                                if isImage {
                                    MemoAuthImageView(urlString: fileUrl, token: viewModel.memosToken)
                                        .frame(maxWidth: .infinity)
                                        .frame(height: 160)
                                        .clipShape(RoundedRectangle(cornerRadius: 8))
                                        .padding(.horizontal, 20)
                                }
                                HStack {
                                    Image(systemName: isImage ? "photo" : "paperclip")
                                    Text(att.filename)
                                    Spacer()
                                    Text(byteCount(att.size)).font(.caption).foregroundStyle(.secondary)
                                }
                                .padding(.horizontal, 20)
                                .padding(.vertical, 2)
                            }
                            .contentShape(Rectangle())
                            .onTapGesture {
                                if let url = URL(string: fileUrl) { NSWorkspace.shared.open(url) }
                            }
                            .padding(.vertical, 2)
                        }
                    }
                    .padding(.vertical, 8)
                    Divider()
                }

                // ── Comments ──────────────────────────────────────────────────
                VStack(alignment: .leading, spacing: 8) {
                    Text("Comments").font(.headline).padding(.horizontal, 20)

                    let memoComments = viewModel.comments[memo.name] ?? []
                    if viewModel.commentLoading.contains(memo.name) {
                        ProgressView().padding(.horizontal, 20)
                    } else {
                        ForEach(memoComments) { comment in
                            MemoCommentRow(comment: comment)
                        }
                    }

                    HStack(alignment: .bottom, spacing: 8) {
                        TextEditor(text: $commentInput)
                            .frame(minHeight: 44, maxHeight: 100)
                            .overlay(RoundedRectangle(cornerRadius: 6).stroke(Color.secondary.opacity(0.3)))

                        Button {
                            let text = commentInput.trimmingCharacters(in: .whitespacesAndNewlines)
                            guard !text.isEmpty else { return }
                            commentInput = ""
                            Task { await viewModel.addComment(to: memo.name, content: text) }
                        } label: {
                            Image(systemName: "paperplane.fill")
                        }
                        .disabled(commentInput.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty)
                        .keyboardShortcut(.return, modifiers: .command)
                        .accessibilityLabel("Send comment")
                    }
                    .padding(.horizontal, 20)
                    .padding(.bottom, 16)
                }
                .padding(.top, 8)
            }
        }
        .navigationTitle(memo.property.title.isEmpty ? "Memo" : memo.property.title)
        .toolbar {
            ToolbarItemGroup(placement: .primaryAction) {
                if memo.property.hasIncompleteTasks {
                    Button { showExtractSheet = true } label: {
                        Label("Extract Tasks", systemImage: "checklist")
                    }
                    .accessibilityLabel("Extract tasks")
                }
                if viewModel.detectFirstDate(in: memo) != nil {
                    Button { showEventSheet = true } label: {
                        Label("Create Event", systemImage: "calendar.badge.plus")
                    }
                    .accessibilityLabel("Create event from memo")
                }
                if !viewModel.configuredRepos.isEmpty {
                    Button { showCommentIssueSheet = true } label: {
                        Label("Comment Issue", systemImage: "bubble.left.and.bubble.right")
                    }
                    .accessibilityLabel("Comment on Gitea issue")
                }
                if !detectedURLs.isEmpty {
                    Button {
                        detectedURLs.forEach { NSWorkspace.shared.open($0) }
                    } label: {
                        Label("Open URL", systemImage: "safari")
                    }
                    .accessibilityLabel("Open links in browser")
                }
                if viewModel.karakeepConfigured && !detectedURLs.isEmpty {
                    Button { showKarakeepSheet = true } label: {
                        Label("Save to Karakeep", systemImage: "bookmark")
                    }
                    .accessibilityLabel("Save links to Karakeep")
                }
                Button {
                    let host = viewModel.memosHost
                    guard !host.isEmpty else { return }
                    let link = "\(host)/\(memo.name)"
                    NSPasteboard.general.clearContents()
                    NSPasteboard.general.setString(link, forType: .string)
                    viewModel.snackbarMessage = "Link copied"
                } label: {
                    Label("Copy Link", systemImage: "link")
                }
                .accessibilityLabel("Copy memo link")
            }
        }
        .task(id: memo.name) {
            await viewModel.loadComments(for: memo.name)
        }
        .overlay(alignment: .bottom) {
            if let msg = viewModel.snackbarMessage {
                Text(msg)
                    .font(.callout)
                    .padding(.horizontal, 16)
                    .padding(.vertical, 8)
                    .background(.regularMaterial)
                    .clipShape(Capsule())
                    .shadow(radius: 4)
                    .padding(.bottom, 20)
                    .transition(.move(edge: .bottom).combined(with: .opacity))
                    .task(id: msg) {
                        try? await Task.sleep(for: .seconds(2))
                        viewModel.clearSnackbar()
                    }
            }
        }
        .animation(.easeInOut(duration: 0.25), value: viewModel.snackbarMessage)
        .sheet(isPresented: $showExtractSheet) {
            ExtractTasksSheetMac(memo: memo, viewModel: viewModel)
        }
        .sheet(isPresented: $showEventSheet) {
            CreateEventFromMemoSheetMac(memo: memo, viewModel: viewModel)
        }
        .sheet(isPresented: $showCommentIssueSheet) {
            CommentOnIssueSheetMac(memo: memo, viewModel: viewModel)
        }
        .sheet(isPresented: $showKarakeepSheet) {
            SaveToKarakeepSheet(urls: detectedURLs, viewModel: viewModel)
        }
    }

    private func byteCount(_ bytes: Int64) -> String {
        ByteCountFormatter.string(fromByteCount: bytes, countStyle: .file)
    }
}

// ─── Authenticated image loader ───────────────────────────────────────────────

/// Fetches an image behind a Bearer-token protected URL and shows it as a thumbnail.
private struct MemoAuthImageView: View {
    let urlString: String
    let token: String

    @State private var image: NSImage? = nil
    @State private var failed = false

    var body: some View {
        ZStack {
            Color(.controlBackgroundColor)
            if let image {
                Image(nsImage: image)
                    .resizable()
                    .aspectRatio(contentMode: .fill)
            } else if failed {
                Image(systemName: "photo.badge.exclamationmark")
                    .font(.title2)
                    .foregroundStyle(.secondary)
            } else {
                ProgressView()
            }
        }
        .task(id: urlString) { await load() }
    }

    private func load() async {
        guard let url = URL(string: urlString) else { failed = true; return }
        var req = URLRequest(url: url, cachePolicy: .returnCacheDataElseLoad, timeoutInterval: 30)
        if !token.isEmpty { req.setValue("Bearer \(token)", forHTTPHeaderField: "Authorization") }
        guard let (data, resp) = try? await URLSession.shared.data(for: req),
              (resp as? HTTPURLResponse)?.statusCode == 200,
              let img = NSImage(data: data) else {
            failed = true; return
        }
        image = img
    }
}

// ─── Comment row ──────────────────────────────────────────────────────────────

struct MemoCommentRow: View {
    let comment: MemosMemo

    var body: some View {
        VStack(alignment: .leading, spacing: 4) {
            let fmt: DateFormatter = {
                let f = DateFormatter(); f.dateStyle = .short; f.timeStyle = .short; return f
            }()
            Text(fmt.string(from: comment.displayTime))
                .font(.caption)
                .foregroundStyle(.secondary)
            Markdown(comment.content)
                .markdownTheme(.gitHub)
                .padding(10)
                .background(Color(.controlBackgroundColor))
                .clipShape(RoundedRectangle(cornerRadius: 8))
        }
        .padding(.horizontal, 20)
        .padding(.vertical, 4)
    }
}
