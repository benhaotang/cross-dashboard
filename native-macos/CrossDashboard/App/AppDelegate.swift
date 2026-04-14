import AppKit
import SwiftUI
import CrossDashboardKit

/// `NSApplicationDelegate` that:
///  1. Registers the app as a Services provider so "Capture to Memos" appears in
///     the right-click Services menu for any text/URL selection.
///  2. Handles the `captureToMemos` service message by opening a floating panel
///     pre-filled with the selected text.
final class AppDelegate: NSObject, NSApplicationDelegate {

    func applicationDidFinishLaunching(_ notification: Notification) {
        NSApplication.shared.servicesProvider = self
    }

    // MARK: - Services handler

    /// Called by macOS when the user picks "Capture to Memos" from the Services menu.
    /// `pboard` contains the selected text/URL; `error` must be set on failure.
    @objc func captureToMemos(_ pboard: NSPasteboard, userData: String?, error: AutoreleasingUnsafeMutablePointer<NSString?>?) {
        var initialText = ""

        if let string = pboard.string(forType: .string) {
            initialText = string
        } else if let urlString = pboard.string(forType: .URL) {
            initialText = urlString
        }

        DispatchQueue.main.async {
            ServiceCapturePanel.show(initialText: initialText)
        }
    }
}

// MARK: - Floating capture panel

/// A lightweight `NSPanel` that mirrors the share extension's compose UI.
/// Uses a callback-based `ServiceComposeView` instead of `NSExtensionContext`.
final class ServiceCapturePanel: NSObject {

    private static var panel: NSPanel?

    static func show(initialText: String) {
        if let existing = panel {
            existing.makeKeyAndOrderFront(nil)
            return
        }

        let view = ServiceComposeView(initialText: initialText) {
            panel?.close()
            panel = nil
        }
        let hosting = NSHostingController(rootView: view)
        hosting.view.frame = NSRect(x: 0, y: 0, width: 480, height: 340)

        let newPanel = NSPanel(
            contentRect: NSRect(x: 0, y: 0, width: 480, height: 340),
            styleMask: [.titled, .closable, .resizable, .nonactivatingPanel],
            backing: .buffered,
            defer: false
        )
        newPanel.title = "Capture to Memos"
        newPanel.contentViewController = hosting
        newPanel.isFloatingPanel = true
        newPanel.center()
        newPanel.makeKeyAndOrderFront(nil)
        panel = newPanel
    }
}

// MARK: - ServiceComposeView

/// Identical compose UI to the share extension but dismisses via `onDone` callback.
private struct ServiceComposeView: View {

    let onDone: () -> Void

    @State private var content: String
    @State private var visibility: MemoVisibility = .private
    @State private var isCapturing = false
    @State private var errorMessage: String? = nil

    init(initialText: String, onDone: @escaping () -> Void) {
        self._content = State(initialValue: initialText)
        self.onDone = onDone
    }

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
                Button("Cancel") { onDone() }
                Button("Capture") {
                    Task { await capture() }
                }
                .buttonStyle(.borderedProminent)
                .disabled(content.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty || isCapturing)
            }
        }
        .padding(20)
    }

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

        guard let body = try? JSONEncoder().encode(
            CreateBody(content: trimmed, visibility: visibility.rawValue)
        ) else { return }
        request.httpBody = body

        do {
            let (_, response) = try await URLSession.shared.data(for: request)
            if (response as? HTTPURLResponse)?.statusCode == 200 {
                onDone()
            } else {
                errorMessage = "Failed to create memo. Check your credentials in Settings."
            }
        } catch {
            errorMessage = error.localizedDescription
        }
    }
}
