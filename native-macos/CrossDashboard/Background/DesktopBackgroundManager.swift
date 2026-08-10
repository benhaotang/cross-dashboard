import AppKit
import Observation
import OSLog
import CrossDashboardKit

@Observable
@MainActor
final class DesktopBackgroundManager {
    static let shared = DesktopBackgroundManager()
    private static let logger = Logger(subsystem: "com.crossdashboard.app", category: "DesktopBackground")
    private let key = "desktop_background_definition"
    var definition: DesktopBackgroundDefinition?
    var lastMessage = "No background snapshot"

    private init() {
        if let data = UserDefaults.standard.data(forKey: key) { definition = try? JSONDecoder().decode(DesktopBackgroundDefinition.self, from: data) }
        NotificationCenter.default.addObserver(forName: NSApplication.didChangeScreenParametersNotification, object: nil, queue: .main) { _ in Task { @MainActor in await Self.shared.refreshIfEnabled() } }
        DistributedNotificationCenter.default().addObserver(forName: Notification.Name("AppleInterfaceThemeChangedNotification"), object: nil, queue: .main) { _ in Task { @MainActor in await Self.shared.applyCurrentAppearance() } }
    }

    func captureInbox(_ vm: InboxViewModel) async {
        definition = DesktopBackgroundDefinition(source: .inbox, inboxType: vm.itemType.rawValue,
            inboxDate: vm.dateFilter.rawValue, searchQuery: vm.searchText.isEmpty ? nil : vm.searchText)
        persist()
        await refreshIfEnabled(reason: "Inbox capture")
    }

    func captureViews(_ vm: ViewsViewModel) async {
        definition = DesktopBackgroundDefinition(source: .views, viewsType: vm.itemType.rawValue,
            viewsDate: vm.dateFilter.rawValue, viewsMode: vm.mode.rawValue)
        persist()
        await refreshIfEnabled(reason: "Views capture")
    }

    func disable() { definition?.enabled = false; persist(); lastMessage = "Automatic updates disabled" }
    func enable() {
        definition?.enabled = true
        persist()
        Task { await refreshIfEnabled(reason: "Enabled in Settings") }
    }

    func refreshIfEnabled(reason: String = "Automatic refresh", force: Bool = false) async {
        guard let definition else {
            lastMessage = "Create a snapshot from Inbox or Views first."
            return
        }
        guard definition.enabled || force else {
            lastMessage = "Automatic updates are disabled."
            return
        }
        lastMessage = "Rendering background…"
        Self.logger.info("Starting background refresh: \(reason, privacy: .public)")
        let content = build(definition)
        do {
            let directory = try FileManager.default.url(for: .picturesDirectory,
                in: .userDomainMask, appropriateFor: nil, create: true)
                .appendingPathComponent("Cross-Dashboard", isDirectory: true)
                .appendingPathComponent("Backgrounds", isDirectory: true)
            try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)
            for screen in NSScreen.screens {
                let id = (screen.deviceDescription[NSDeviceDescriptionKey("NSScreenNumber")] as? NSNumber)?.stringValue ?? "main"
                let pixels = CGSize(width: screen.frame.width * screen.backingScaleFactor, height: screen.frame.height * screen.backingScaleFactor)
                for dark in [false, true] {
                    guard let data = DesktopBackgroundRenderer.render(content, pixels: pixels, dark: dark) else {
                        throw DesktopBackgroundError.renderFailed(id)
                    }
                    let url = directory.appendingPathComponent("\(id)_\(dark ? "dark" : "light").png")
                    try data.write(to: url, options: .atomic)
                    Self.logger.debug("Rendered \(url.path, privacy: .public)")
                }
            }
            try apply(directory: directory)
            lastMessage = "Updated \(Date().formatted(date: .omitted, time: .shortened))"
            Self.logger.info("Desktop background refresh completed")
        } catch {
            lastMessage = "Background error: \(error.localizedDescription)"
            Self.logger.error("Background refresh failed: \(error.localizedDescription, privacy: .public)")
        }
    }

    private func applyCurrentAppearance() async { await refreshIfEnabled() }
    private func apply(directory: URL) throws {
        let dark = UserDefaults.standard.string(forKey: "AppleInterfaceStyle") == "Dark"
        for screen in NSScreen.screens {
            let id = (screen.deviceDescription[NSDeviceDescriptionKey("NSScreenNumber")] as? NSNumber)?.stringValue ?? "main"
            let url = directory.appendingPathComponent("\(id)_\(dark ? "dark" : "light").png")
            guard FileManager.default.fileExists(atPath: url.path) else {
                throw DesktopBackgroundError.missingRenderedFile(url)
            }
            var options = NSWorkspace.shared.desktopImageOptions(for: screen) ?? [:]
            options[.imageScaling] = NSImageScaling.scaleProportionallyUpOrDown.rawValue; options[.allowClipping] = true
            try NSWorkspace.shared.setDesktopImageURL(url, for: screen, options: options)
            let applied = NSWorkspace.shared.desktopImageURL(for: screen)?.standardizedFileURL
            guard applied == url.standardizedFileURL else {
                throw DesktopBackgroundError.notApplied(expected: url, actual: applied)
            }
            Self.logger.info("Applied \(url.path, privacy: .public) to display \(id, privacy: .public)")
        }
    }

    private func build(_ d: DesktopBackgroundDefinition) -> DesktopBackgroundContent {
        if d.source == .inbox {
            let vm = InboxViewModel(); vm.itemType = .init(rawValue: d.inboxType) ?? .all; vm.dateFilter = .init(rawValue: d.inboxDate) ?? .all; vm.searchText = d.searchQuery ?? ""
            let rows = vm.filteredItems.map { item -> DesktopBackgroundRow in
                switch item {
                case .event(let event, _):
                    .init(
                        title: event.summary,
                        subtitle: event.start.formatted(date: .abbreviated, time: .shortened),
                        group: nil,
                        kind: 0,
                        overdue: false
                    )
                case .task(let task, _):
                    .init(
                        title: task.summary,
                        subtitle: task.due?.formatted(date: .abbreviated, time: .shortened) ?? "No due date",
                        group: nil,
                        kind: 1,
                        overdue: (task.due ?? .distantFuture) < Date()
                    )
                case .issue(let issue, _):
                    .init(title: issue.title, subtitle: issue.repository, group: nil, kind: 2, overdue: false)
                }
            }
            let filters = "\(d.inboxType) · \(d.inboxDate)" + (d.searchQuery.map { " · Search “\($0)”" } ?? "")
            return .init(title:"INBOX",filters:filters,mode:nil,rows:rows,
                totalMinutes: vm.totalEstimatedMinutes)
        }
        let vm = ViewsViewModel(); vm.itemType = .init(rawValue:d.viewsType) ?? .all; vm.dateFilter = .init(rawValue:d.viewsDate) ?? .all; vm.mode = .init(rawValue:d.viewsMode) ?? .kanban
        var rows: [DesktopBackgroundRow] = []
        let urgentCutoff = Calendar.current.date(byAdding: .day, value: 2, to: Date()) ?? Date()
        for task in vm.allTasks {
            let group: String
            if d.viewsMode == "Covey" {
                let important = task.priority > 0 && task.priority <= 4
                let urgent = task.due.map { $0 <= urgentCutoff } ?? false
                group = important ? (urgent ? "Do First" : "Schedule") : (urgent ? "Delegate" : "Eliminate")
            } else {
                group = vm.kanbanColumns.first { column in task.categories.contains { $0.caseInsensitiveCompare(column) == .orderedSame } } ?? "Untagged"
            }
            rows.append(.init(
                title: task.summary,
                subtitle: task.due?.formatted(date: .abbreviated, time: .shortened) ?? "Task",
                group: group,
                kind: 1,
                overdue: (task.due ?? .distantFuture) < Date()
            ))
        }
        for issue in vm.allIssues {
            let group: String
            if d.viewsMode == "Covey" {
                let labels = Set(issue.labels.map { $0.lowercased() })
                group = labels.contains("do") ? "Do First" : labels.contains("delay") ? "Schedule" : labels.contains("delegate") ? "Delegate" : labels.contains("eliminate") ? "Eliminate" : "Untagged"
            } else {
                group = vm.kanbanColumns.first { column in issue.labels.contains { $0.caseInsensitiveCompare(column) == .orderedSame } } ?? "Untagged"
            }
            if d.viewsMode != "Covey" || group != "Untagged" {
                rows.append(.init(title: issue.title, subtitle: issue.repository, group: group, kind: 2, overdue: false))
            }
        }
        let groups = d.viewsMode == "Covey"
            ? ["Do First", "Schedule", "Delegate", "Eliminate"]
            : (["Untagged"] + vm.kanbanColumns)
        return .init(title:"VIEWS",filters:"\(d.viewsType) · \(d.viewsDate)",mode:d.viewsMode,
            groups:groups,rows:rows)
    }

    private func persist() {
        if let definition, let data = try? JSONEncoder().encode(definition) {
            UserDefaults.standard.set(data, forKey: key)
        }
    }
}

private enum DesktopBackgroundError: LocalizedError {
    case renderFailed(String)
    case missingRenderedFile(URL)
    case notApplied(expected: URL, actual: URL?)

    var errorDescription: String? {
        switch self {
        case .renderFailed(let display): return "Could not render display \(display)."
        case .missingRenderedFile(let url): return "Rendered image is missing at \(url.path)."
        case .notApplied(let expected, let actual):
            return "macOS did not select \(expected.lastPathComponent); current image is \(actual?.lastPathComponent ?? "unknown")."
        }
    }
}
