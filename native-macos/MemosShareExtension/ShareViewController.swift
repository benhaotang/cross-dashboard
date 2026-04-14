import Cocoa
import SwiftUI
import UniformTypeIdentifiers
import CrossDashboardKit

@objc(ShareViewController)
final class ShareViewController: NSViewController {

    override func loadView() {
        view = NSView(frame: NSRect(x: 0, y: 0, width: 480, height: 380))
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

// MARK: - Pending file

private struct PendingFile {
    let filename: String
    let mimeType: String
    let data: Data
}

// MARK: - Compose View

private struct ShareComposeView: View {

    let extensionContext: NSExtensionContext?

    @State private var content = ""
    @State private var visibility: MemoVisibility = .private
    @State private var isCapturing = false
    @State private var errorMessage: String? = nil
    @State private var pendingFile: PendingFile? = nil
    @State private var editedFilename: String = ""

    var body: some View {
        VStack(alignment: .leading, spacing: 16) {
            Text("Capture to Memos")
                .font(.title2)
                .fontWeight(.semibold)

            if let file = pendingFile {
                VStack(alignment: .leading, spacing: 4) {
                    HStack(spacing: 6) {
                        Image(systemName: "paperclip").foregroundStyle(.secondary)
                        TextField("Filename", text: $editedFilename)
                            .textFieldStyle(.roundedBorder)
                            .font(.caption)
                        Text(ByteCountFormatter.string(fromByteCount: Int64(file.data.count), countStyle: .file))
                            .font(.caption2).foregroundStyle(.tertiary)
                    }
                }
                .padding(8)
                .background(.quaternary, in: RoundedRectangle(cornerRadius: 6))
                .onAppear { editedFilename = file.filename }
            }

            TextEditor(text: $content)
                .frame(minHeight: 100)
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
                Button(isCapturing ? "Capturing…" : "Capture") {
                    Task { await capture() }
                }
                .buttonStyle(.borderedProminent)
                .disabled(
                    (content.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty && pendingFile == nil)
                    || isCapturing
                )
            }
        }
        .padding(20)
        .task { await loadPayload() }
    }

    // MARK: - Load shared payload

    private func loadPayload() async {
        guard let items = extensionContext?.inputItems as? [NSExtensionItem] else {
            print("[ShareExt] loadPayload: no input items")
            return
        }
        print("[ShareExt] loadPayload: \(items.count) NSExtensionItem(s)")
        var textParts: [String] = []

        for (i, item) in items.enumerated() {
            let providers = item.attachments ?? []
            print("[ShareExt] item[\(i)]: \(providers.count) provider(s)")
            for (j, provider) in providers.enumerated() {
                print("[ShareExt]   provider[\(j)] registeredTypeIdentifiers: \(provider.registeredTypeIdentifiers)")
                if provider.hasItemConformingToTypeIdentifier("public.file-url") {
                    if let file = await loadFile(from: provider) {
                        print("[ShareExt]   → loaded file: \(file.filename) \(file.mimeType) \(file.data.count) bytes")
                        pendingFile = file
                    } else {
                        print("[ShareExt]   → loadFile returned nil")
                    }
                } else if let text = await loadText(from: provider) {
                    print("[ShareExt]   → loaded text: \(text.prefix(120))")
                    textParts.append(text)
                } else {
                    print("[ShareExt]   → could not load item (no matching type)")
                }
            }
        }
        content = textParts.joined(separator: "\n")
    }

    private func loadFile(from provider: NSItemProvider) async -> PendingFile? {
        let allTypes = provider.registeredTypeIdentifiers
        print("[ShareExt] loadFile: registeredTypeIdentifiers = \(allTypes)")

        // Step 1 — get the original filename from public.file-url.
        // loadFileRepresentation copies the file to a macOS temp dir and names it after
        // the UTI description (e.g. "rich text (RTF).rtf"), not the real filename.
        // Loading public.file-url gives us the original path so lastPathComponent is correct.
        let originalFilename = await loadOriginalFilename(from: provider)
        print("[ShareExt] loadFile: originalFilename = \(originalFilename ?? "nil")")

        // Step 2 — read file bytes.
        let urlOnlyTypes: Set<String> = ["public.url", "public.file-url",
                                         "com.apple.finder.node", "public.vcard"]
        let contentTypeId = allTypes.first { !urlOnlyTypes.contains($0) }
        print("[ShareExt] loadFile: selected contentTypeId = \(contentTypeId ?? "nil")")

        if let typeId = contentTypeId {
            if let file = await loadViaFileRepresentation(provider: provider, typeId: typeId,
                                                          originalFilename: originalFilename) {
                return file
            }
            print("[ShareExt] loadFile: loadFileRepresentation failed, trying file-url fallback")
        }

        if provider.hasItemConformingToTypeIdentifier("public.file-url") {
            return await loadViaSecurityScopedURL(provider: provider, originalFilename: originalFilename)
        }

        print("[ShareExt] loadFile: all strategies failed")
        return nil
    }

    /// Load the original file URL from the item provider and return its last path component.
    /// This is the only reliable source of the real filename on macOS — suggestedName is
    /// often nil for Finder shares, and loadFileRepresentation uses a UTI-derived name.
    private func loadOriginalFilename(from provider: NSItemProvider) async -> String? {
        guard provider.hasItemConformingToTypeIdentifier("public.file-url") else { return nil }
        return await withCheckedContinuation { continuation in
            provider.loadItem(forTypeIdentifier: "public.file-url", options: nil) { item, _ in
                let url: URL?
                if let u = item as? URL { url = u }
                else if let ns = item as? NSURL { url = ns as URL }
                else if let data = item as? Data { url = URL(dataRepresentation: data, relativeTo: nil) }
                else { url = nil }
                continuation.resume(returning: url?.lastPathComponent)
            }
        }
    }

    private func loadViaFileRepresentation(provider: NSItemProvider, typeId: String,
                                            originalFilename: String?) async -> PendingFile? {
        return await withCheckedContinuation { continuation in
            provider.loadFileRepresentation(forTypeIdentifier: typeId) { url, error in
                if let error {
                    print("[ShareExt] loadFileRepresentation(\(typeId)) error: \(error)")
                    continuation.resume(returning: nil)
                    return
                }
                guard let url = url else {
                    print("[ShareExt] loadFileRepresentation(\(typeId)): url is nil")
                    continuation.resume(returning: nil)
                    return
                }
                do {
                    let data = try Data(contentsOf: url)
                    let ext = url.pathExtension
                    let mime = UTType(filenameExtension: ext)?.preferredMIMEType
                           ?? UTType(typeId)?.preferredMIMEType
                           ?? "application/octet-stream"
                    // Use originalFilename (from public.file-url) as it holds the real name.
                    // Fall back to the temp URL's last component only as last resort.
                    let filename = originalFilename ?? url.lastPathComponent
                    print("[ShareExt] loadFileRepresentation: \(data.count) bytes, mime=\(mime), filename=\(filename)")
                    continuation.resume(returning: PendingFile(filename: filename, mimeType: mime, data: data))
                } catch {
                    print("[ShareExt] loadFileRepresentation: Data(contentsOf:) failed: \(error)")
                    continuation.resume(returning: nil)
                }
            }
        }
    }

    private func loadViaSecurityScopedURL(provider: NSItemProvider, originalFilename: String?) async -> PendingFile? {
        await withCheckedContinuation { continuation in
            provider.loadItem(forTypeIdentifier: "public.file-url", options: nil) { item, error in
                if let error { print("[ShareExt] loadItem(file-url) error: \(error)") }
                let url: URL?
                if let u = item as? URL { url = u }
                else if let ns = item as? NSURL { url = ns as URL }
                else {
                    print("[ShareExt] loadItem(file-url): item type=\(type(of: item)) — not URL")
                    url = nil
                }
                guard let fileURL = url else { continuation.resume(returning: nil); return }

                print("[ShareExt] loadItem(file-url): \(fileURL)")
                let accessing = fileURL.startAccessingSecurityScopedResource()
                print("[ShareExt] startAccessingSecurityScopedResource = \(accessing)")
                defer { if accessing { fileURL.stopAccessingSecurityScopedResource() } }

                do {
                    let data = try Data(contentsOf: fileURL)
                    let ext = fileURL.pathExtension
                    let mime = UTType(filenameExtension: ext)?.preferredMIMEType ?? "application/octet-stream"
                    let filename = originalFilename ?? fileURL.lastPathComponent
                    print("[ShareExt] security-scoped read: \(data.count) bytes, mime=\(mime), filename=\(filename)")
                    continuation.resume(returning: PendingFile(filename: filename, mimeType: mime, data: data))
                } catch {
                    print("[ShareExt] security-scoped Data(contentsOf:) failed: \(error)")
                    continuation.resume(returning: nil)
                }
            }
        }
    }

    private func loadText(from provider: NSItemProvider) async -> String? {
        if provider.hasItemConformingToTypeIdentifier("public.url") {
            return await withCheckedContinuation { continuation in
                provider.loadItem(forTypeIdentifier: "public.url", options: nil) { item, _ in
                    let url: URL?
                    if let u = item as? URL { url = u }
                    else if let ns = item as? NSURL { url = ns as URL }
                    else { url = nil }
                    continuation.resume(returning: url?.absoluteString)
                }
            }
        }
        if provider.hasItemConformingToTypeIdentifier("public.plain-text") {
            return await withCheckedContinuation { continuation in
                provider.loadItem(forTypeIdentifier: "public.plain-text", options: nil) { item, _ in
                    continuation.resume(returning: item as? String)
                }
            }
        }
        return nil
    }

    // MARK: - Capture

    private func capture() async {
        let trimmedText = content.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !trimmedText.isEmpty || pendingFile != nil else { return }

        isCapturing = true
        errorMessage = nil
        defer { isCapturing = false }

        let keychain = KeychainStore.shared
        guard let host = keychain.get(CredentialKey.memosHost), !host.isEmpty,
              let token = keychain.get(CredentialKey.memosToken), !token.isEmpty else {
            errorMessage = "Memos host not configured."
            return
        }
        print("[ShareExt] capture: host=\(host)")

        do {
            // 1. Upload attachment first, get its resource name.
            //    Use the user-edited filename (may have been renamed in the compose UI).
            var attachmentName: String? = nil
            if var file = pendingFile {
                let finalName = editedFilename.trimmingCharacters(in: .whitespacesAndNewlines)
                if !finalName.isEmpty { file = PendingFile(filename: finalName, mimeType: file.mimeType, data: file.data) }
                print("[ShareExt] ── Step 1: upload attachment '\(file.filename)' ──")
                attachmentName = try await uploadAttachment(file, host: host, token: token)
                print("[ShareExt] attachmentName = \(attachmentName ?? "nil")")
            }

            // 2. Create memo with attachment embedded in the body
            print("[ShareExt] ── Step 2: create memo (attachmentName=\(attachmentName ?? "none")) ──")
            let memoContent = trimmedText.isEmpty ? (editedFilename.isEmpty ? (pendingFile?.filename ?? "") : editedFilename) : trimmedText
            let memoName = try await createMemo(
                content: memoContent,
                visibility: visibility,
                attachmentName: attachmentName,
                host: host,
                token: token
            )
            print("[ShareExt] memoName = \(memoName)")

            // 3. Belt-and-suspenders: also call SetMemoAttachments
            if let attachmentName, let file = pendingFile {
                print("[ShareExt] ── Step 3: setMemoAttachments ──")
                do {
                    try await setMemoAttachments(
                        memoName: memoName,
                        attachmentName: attachmentName,
                        file: file,
                        host: host,
                        token: token
                    )
                } catch {
                    // Non-fatal — memo was already created with attachment in body
                    print("[ShareExt] setMemoAttachments failed (non-fatal): \(error)")
                }
            }

            extensionContext?.completeRequest(returningItems: [], completionHandler: nil)
        } catch {
            print("[ShareExt] capture FAILED: \(error)")
            errorMessage = error.localizedDescription
        }
    }

    // MARK: - API helpers

    private func uploadAttachment(_ file: PendingFile, host: String, token: String) async throws -> String {
        let urlString = "\(host)/api/v1/attachments"
        print("[ShareExt] POST \(urlString)")
        guard let url = URL(string: urlString) else { throw URLError(.badURL) }

        var req = URLRequest(url: url)
        req.httpMethod = "POST"
        req.setValue("Bearer \(token)", forHTTPHeaderField: "Authorization")
        req.setValue("application/json", forHTTPHeaderField: "Content-Type")

        let bodyObj: [String: Any] = [
            "filename": file.filename,
            "type": file.mimeType,
            "content": file.data.base64EncodedString(),
        ]
        req.httpBody = try JSONSerialization.data(withJSONObject: bodyObj)
        print("[ShareExt]   body keys: filename=\(file.filename) type=\(file.mimeType) content=<\(file.data.count) bytes base64>")

        let (data, response) = try await URLSession.shared.data(for: req)
        let status = (response as? HTTPURLResponse)?.statusCode ?? -1
        let raw = String(data: data, encoding: .utf8) ?? "<binary \(data.count) bytes>"
        print("[ShareExt]   → HTTP \(status)")
        print("[ShareExt]   → body: \(raw)")

        guard status == 200 else {
            throw NSError(domain: "com.crossdashboard", code: status,
                          userInfo: [NSLocalizedDescriptionKey: "uploadAttachment HTTP \(status): \(raw)"])
        }

        // Parse "name" from response — may be at top level or nested
        guard let json = try? JSONSerialization.jsonObject(with: data) as? [String: Any] else {
            throw NSError(domain: "com.crossdashboard", code: -2,
                          userInfo: [NSLocalizedDescriptionKey: "uploadAttachment: non-JSON response: \(raw)"])
        }
        print("[ShareExt]   → parsed JSON keys: \(json.keys.sorted())")

        guard let name = json["name"] as? String else {
            throw NSError(domain: "com.crossdashboard", code: -3,
                          userInfo: [NSLocalizedDescriptionKey: "uploadAttachment: no 'name' in response: \(raw)"])
        }
        return name
    }

    private func createMemo(
        content: String,
        visibility: MemoVisibility,
        attachmentName: String?,
        host: String,
        token: String
    ) async throws -> String {
        let urlString = "\(host)/api/v1/memos"
        print("[ShareExt] POST \(urlString)")
        guard let url = URL(string: urlString) else { throw URLError(.badURL) }

        var req = URLRequest(url: url)
        req.httpMethod = "POST"
        req.setValue("Bearer \(token)", forHTTPHeaderField: "Authorization")
        req.setValue("application/json", forHTTPHeaderField: "Content-Type")

        var bodyObj: [String: Any] = [
            "content": content,
            "visibility": visibility.rawValue,
            "state": "NORMAL",
        ]
        if let attachmentName {
            bodyObj["attachments"] = [["name": attachmentName]]
        }
        req.httpBody = try JSONSerialization.data(withJSONObject: bodyObj)

        let bodyStr = String(data: req.httpBody!, encoding: .utf8) ?? ""
        print("[ShareExt]   body: \(bodyStr)")

        let (data, response) = try await URLSession.shared.data(for: req)
        let status = (response as? HTTPURLResponse)?.statusCode ?? -1
        let raw = String(data: data, encoding: .utf8) ?? "<binary \(data.count) bytes>"
        print("[ShareExt]   → HTTP \(status)")
        print("[ShareExt]   → body: \(raw)")

        guard status == 200 else {
            throw NSError(domain: "com.crossdashboard", code: status,
                          userInfo: [NSLocalizedDescriptionKey: "createMemo HTTP \(status): \(raw)"])
        }

        guard let json = try? JSONSerialization.jsonObject(with: data) as? [String: Any] else {
            throw NSError(domain: "com.crossdashboard", code: -2,
                          userInfo: [NSLocalizedDescriptionKey: "createMemo: non-JSON response: \(raw)"])
        }
        print("[ShareExt]   → parsed JSON keys: \(json.keys.sorted())")
        print("[ShareExt]   → attachments in response: \(json["attachments"] ?? "KEY MISSING")")

        guard let name = json["name"] as? String else {
            throw NSError(domain: "com.crossdashboard", code: -3,
                          userInfo: [NSLocalizedDescriptionKey: "createMemo: no 'name' in response: \(raw)"])
        }
        return name
    }

    private func setMemoAttachments(
        memoName: String,
        attachmentName: String,
        file: PendingFile,
        host: String,
        token: String
    ) async throws {
        // memoName = "memos/{id}" — use as-is in the path: /api/v1/memos/{id}/attachments
        let urlString = "\(host)/api/v1/\(memoName)/attachments"
        print("[ShareExt] PATCH \(urlString)")
        guard let url = URL(string: urlString) else { throw URLError(.badURL) }

        var req = URLRequest(url: url)
        req.httpMethod = "PATCH"
        req.setValue("Bearer \(token)", forHTTPHeaderField: "Authorization")
        req.setValue("application/json", forHTTPHeaderField: "Content-Type")

        let bodyObj: [String: Any] = [
            "name": memoName,
            "attachments": [["name": attachmentName, "filename": file.filename, "type": file.mimeType]],
        ]
        req.httpBody = try JSONSerialization.data(withJSONObject: bodyObj)
        let bodyStr = String(data: req.httpBody!, encoding: .utf8) ?? ""
        print("[ShareExt]   body: \(bodyStr)")

        let (data, response) = try await URLSession.shared.data(for: req)
        let status = (response as? HTTPURLResponse)?.statusCode ?? -1
        let raw = String(data: data, encoding: .utf8) ?? "<binary \(data.count) bytes>"
        print("[ShareExt]   → HTTP \(status)")
        print("[ShareExt]   → body: \(raw)")

        guard (200...204).contains(status) else {
            throw NSError(domain: "com.crossdashboard", code: status,
                          userInfo: [NSLocalizedDescriptionKey: "setMemoAttachments HTTP \(status): \(raw)"])
        }
    }
}
