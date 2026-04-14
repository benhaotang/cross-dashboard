import Cocoa
import SwiftUI
import CrossDashboardKit

/// macOS Share extension entry point.
/// Reads text/URL from the share payload, shows a compose UI, then POSTs
/// directly to the Memos API — no dependency on the main-app MemosClient.
@objc(ShareViewController)
final class ShareViewController: NSViewController {

    override func loadView() {
        view = NSView(frame: NSRect(x: 0, y: 0, width: 480, height: 340))
    }

    override func viewDidLoad() {
        super.viewDidLoad()

        let hostingController = NSHostingController(
            rootView: ShareComposeView(extensionContext: extensionContext)
        )
        addChild(hostingController)
        hostingController.view.frame = view.bounds
        hostingController.view.autoresizingMask = [.width, .height]
        view.addSubview(hostingController.view)
    }
}

// ─── Compose View ─────────────────────────────────────────────────────────────

private struct ShareComposeView: View {

    let extensionContext: NSExtensionContext?

    @State private var content = ""
    @State private var visibility: MemoVisibility = .private
    @State private var isCapturing = false
    @State private var errorMessage: String? = nil

    var body: some View {
        VStack(alignment: .leading, spacing: 16) {
            Text("Capture to Memos")
                .font(.title2)
                .fontWeight(.semibold)

            TextEditor(text: $content)
                .frame(minHeight: 120)
                .overlay(RoundedRectangle(cornerRadius: 6).stroke(Color.secondary.opacity(0.3)))

            Picker("Visibility", selection: $visibility) {
                Text("Private").tag(MemoVisibility.private)
                Text("Protected").tag(MemoVisibility.protected_)
                Text("Public").tag(MemoVisibility.public)
            }
            .pickerStyle(.segmented)

            if let errorMessage {
                Text(errorMessage).foregroundStyle(.red).font(.caption)
            }

            HStack {
                Spacer()
                Button("Cancel") {
                    extensionContext?.cancelRequest(withError: NSError(domain: "com.crossdashboard", code: 0))
                }
                Button("Capture") {
                    Task { await capture() }
                }
                .buttonStyle(.borderedProminent)
                .disabled(content.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty || isCapturing)
            }
        }
        .padding(20)
        .task { await loadPayload() }
    }

    // ─── Load shared items ────────────────────────────────────────────────────

    private func loadPayload() async {
        guard let items = extensionContext?.inputItems as? [NSExtensionItem] else { return }
        var parts: [String] = []
        for item in items {
            for provider in (item.attachments ?? []) {
                if provider.hasItemConformingToTypeIdentifier("public.file-url") {
                    // Files shared from Finder — use their path as a reference
                    if let fileURL = try? await provider.loadItem(forTypeIdentifier: "public.file-url") as? URL {
                        parts.append(fileURL.path)
                    }
                } else if provider.hasItemConformingToTypeIdentifier("public.url") {
                    if let url = try? await provider.loadItem(forTypeIdentifier: "public.url") as? URL {
                        parts.append(url.absoluteString)
                    }
                } else if provider.hasItemConformingToTypeIdentifier("public.plain-text") {
                    if let text = try? await provider.loadItem(forTypeIdentifier: "public.plain-text") as? String {
                        parts.append(text)
                    }
                }
            }
        }
        content = parts.joined(separator: "\n")
    }

    // ─── Post to Memos API directly (no MemosClient dependency) ──────────────

    private func capture() async {
        let trimmed = content.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !trimmed.isEmpty else { return }

        isCapturing = true
        defer { isCapturing = false }

        let keychain = KeychainStore.shared
        guard let host = keychain.get(CredentialKey.memosHost), !host.isEmpty,
              let token = keychain.get(CredentialKey.memosToken), !token.isEmpty,
              let url = URL(string: "\(host)/api/v1/memos") else {
            errorMessage = "Memos host not configured. Open CrossDashboard → Settings → Memos."
            return
        }

        var request = URLRequest(url: url)
        request.httpMethod = "POST"
        request.setValue("Bearer \(token)", forHTTPHeaderField: "Authorization")
        request.setValue("application/json", forHTTPHeaderField: "Content-Type")

        struct CreateBody: Encodable {
            let content: String
            let visibility: String
        }

        guard let body = try? JSONEncoder().encode(CreateBody(content: trimmed, visibility: visibility.rawValue)) else { return }
        request.httpBody = body

        do {
            let (_, response) = try await URLSession.shared.data(for: request)
            if (response as? HTTPURLResponse)?.statusCode == 200 {
                extensionContext?.completeRequest(returningItems: [], completionHandler: nil)
            } else {
                errorMessage = "Failed to create memo. Check your credentials in Settings."
            }
        } catch {
            errorMessage = error.localizedDescription
        }
    }
}
