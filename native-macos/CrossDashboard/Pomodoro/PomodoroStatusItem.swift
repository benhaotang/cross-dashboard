import AppKit
import SwiftUI
import CrossDashboardKit

/// Manages an `NSStatusItem` in the macOS menu bar that shows the Pomodoro
/// countdown while the timer is active.
///
/// Clicking the status item opens an `NSPopover` that hosts `PomodoroBarView`.
/// This is the macOS equivalent of the Android `PomodoroForegroundService`
/// live notification chip.
@MainActor
final class PomodoroStatusItem: NSObject {

    static let shared = PomodoroStatusItem()

    private var statusItem: NSStatusItem?
    private let popover = NSPopover()
    private var updateTimer: Timer?

    private override init() {
        super.init()
        popover.behavior = .transient
        popover.animates = true
    }

    // ─── Lifecycle ────────────────────────────────────────────────────────────

    func show() {
        guard AppPreferences.shared.showPomodoroInMenuBar else { return }
        if statusItem == nil {
            statusItem = NSStatusBar.system.statusItem(withLength: NSStatusItem.variableLength)
            statusItem?.button?.target = self
            statusItem?.button?.action = #selector(togglePopover(_:))
            statusItem?.button?.sendAction(on: [.leftMouseUp, .rightMouseUp])
        }
        // Wire the SwiftUI popover content
        let contentView = PomodoroBarView()
            .environment(PomodoroViewModel.shared)
        let host = NSHostingView(rootView: contentView)
        host.setFrameSize(NSSize(width: 300, height: 220))
        let vc = NSViewController()
        vc.view = host
        popover.contentViewController = vc
        popover.contentSize = host.frame.size

        startTitleUpdates()
        updateTitle()
    }

    func hide() {
        popover.close()
        stopTitleUpdates()
        if let item = statusItem {
            NSStatusBar.system.removeStatusItem(item)
            statusItem = nil
        }
    }

    // ─── Title updates ────────────────────────────────────────────────────────

    private func startTitleUpdates() {
        updateTimer?.invalidate()
        updateTimer = Timer.scheduledTimer(withTimeInterval: 1, repeats: true) { [weak self] _ in
            Task { @MainActor [weak self] in self?.updateTitle() }
        }
        RunLoop.main.add(updateTimer!, forMode: .common)
    }

    private func stopTitleUpdates() {
        updateTimer?.invalidate()
        updateTimer = nil
    }

    private func updateTitle() {
        let vm = PomodoroViewModel.shared
        guard vm.state.active else {
            hide()
            return
        }
        statusItem?.button?.title = vm.menuBarTitle
    }

    // ─── Popover toggle ───────────────────────────────────────────────────────

    @objc private func togglePopover(_ sender: NSStatusBarButton) {
        if popover.isShown {
            popover.close()
        } else if let button = statusItem?.button {
            popover.show(relativeTo: button.bounds, of: button, preferredEdge: .minY)
        }
    }
}
