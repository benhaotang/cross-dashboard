import AppKit
import CrossDashboardKit
import Foundation
import OSLog

@MainActor
final class AgentDesktopBackgroundRefresher {
    static let shared = AgentDesktopBackgroundRefresher()

    private static let logger = Logger(
        subsystem: "com.crossdashboard.app.background-agent",
        category: "DesktopBackground"
    )
    private let defaults = UserDefaults(suiteName: AppPreferences.appGroupSuiteName) ?? .standard

    func refreshIfEnabled(
        events: [CalendarEvent],
        tasks: [CalDavTask],
        issues: [GiteaIssue]
    ) async {
        guard
            let data = defaults.data(forKey: "desktop_background_definition"),
            let definition = try? JSONDecoder().decode(DesktopBackgroundDefinition.self, from: data),
            definition.enabled
        else { return }

        do {
            let directory = try backgroundDirectory()
            let content = build(definition, events: events, tasks: tasks, issues: issues)
            let bothPath = defaults.string(forKey: "desktop_background_image_path")
            let lightPath = defaults.string(forKey: "desktop_background_light_image_path")
            let darkPath = defaults.string(forKey: "desktop_background_dark_image_path")
            let opacity = min(
                1,
                max(0.5, defaults.object(forKey: "desktop_background_glass_opacity") as? Double ?? 0.8)
            )
            let fit = DesktopBackgroundImageFit(
                rawValue: defaults.string(forKey: "desktop_background_image_fit") ?? ""
            ) ?? .fill
            let accent = NSColor.controlAccentColor.usingColorSpace(.sRGB) ?? .controlAccentColor

            for screen in NSScreen.screens {
                let id = screenID(screen)
                let pixels = CGSize(
                    width: screen.frame.width * screen.backingScaleFactor,
                    height: screen.frame.height * screen.backingScaleFactor
                )
                for dark in [false, true] {
                    let specificPath = dark ? darkPath : lightPath
                    let appearance = DesktopBackgroundAppearance(
                        imageURL: (specificPath ?? bothPath).map { URL(fileURLWithPath: $0) },
                        usesContainerAppearanceVariants: specificPath == nil && bothPath != nil,
                        glassOpacity: CGFloat(opacity),
                        imageFit: fit
                    )
                    guard let image = DesktopBackgroundRenderer.render(
                        content,
                        pixels: pixels,
                        dark: dark,
                        appearance: appearance,
                        accent: accent
                    ) else { continue }
                    try image.write(
                        to: directory.appendingPathComponent("\(id)_\(dark ? "dark" : "light").png"),
                        options: .atomic
                    )
                }
            }
            try apply(directory: directory)
            Self.logger.info("Desktop background refreshed by agent")
        } catch {
            Self.logger.error("Desktop background refresh failed: \(error.localizedDescription, privacy: .public)")
        }
    }

    private func build(
        _ definition: DesktopBackgroundDefinition,
        events: [CalendarEvent],
        tasks: [CalDavTask],
        issues: [GiteaIssue]
    ) -> DesktopBackgroundContent {
        if definition.source == .inbox {
            return buildInbox(definition, events: events, tasks: tasks, issues: issues)
        }
        return buildViews(definition, tasks: tasks, issues: issues)
    }

    private func buildInbox(
        _ definition: DesktopBackgroundDefinition,
        events: [CalendarEvent],
        tasks: [CalDavTask],
        issues: [GiteaIssue]
    ) -> DesktopBackgroundContent {
        let now = Date()
        let horizon = Calendar.current.date(byAdding: .day, value: 7, to: now) ?? now
        var items: [InboxBackgroundItem] = events
            .filter { $0.start >= now && $0.start <= horizon }
            .sorted { $0.start < $1.start }
            .map { .event($0) }
        items += tasks
            .filter {
                $0.status != .completed && $0.status != .cancelled &&
                $0.due.map { $0 <= horizon } == true
            }
            .sorted { ($0.due ?? .distantFuture) < ($1.due ?? .distantFuture) }
            .map { .task($0) }
        items += issues.filter { $0.state == "open" }.prefix(20).map { .issue($0) }

        items = items.filter { item in
            switch (definition.inboxType, item) {
            case ("Events", .event), ("Tasks", .task), ("Issues", .issue), ("All", _): true
            default: false
            }
        }
        items = items.filter { matchesDate($0.date, filter: definition.inboxDate) }
        if let query = definition.searchQuery, !query.isEmpty {
            items = items.filter { $0.title.localizedCaseInsensitiveContains(query) }
        }

        let rows = items.map { item in
            DesktopBackgroundRow(
                title: item.title,
                subtitle: item.subtitle,
                group: nil,
                kind: item.kind,
                overdue: item.overdue
            )
        }
        let filters = "\(definition.inboxType) · \(definition.inboxDate)" +
            (definition.searchQuery.map { " · Search \"\($0)\"" } ?? "")
        return DesktopBackgroundContent(
            title: "INBOX",
            filters: filters,
            mode: nil,
            rows: rows,
            totalMinutes: items.reduce(0) { $0 + $1.estimatedMinutes }
        )
    }

    private func buildViews(
        _ definition: DesktopBackgroundDefinition,
        tasks: [CalDavTask],
        issues: [GiteaIssue]
    ) -> DesktopBackgroundContent {
        let columns = AppPreferences.shared.kanbanColumns
        let visibleTasks = definition.viewsType == "Issues" ? [] : tasks.filter {
            $0.status != .completed && $0.status != .cancelled &&
            matchesDate($0.due, filter: definition.viewsDate)
        }
        let visibleIssues = definition.viewsType == "Tasks" ? [] : issues.filter {
            $0.state == "open" && matchesDate($0.milestoneDueOn, filter: definition.viewsDate)
        }
        let urgentCutoff = Calendar.current.date(byAdding: .day, value: 2, to: Date()) ?? Date()
        var rows = visibleTasks.map { task -> DesktopBackgroundRow in
            let group: String
            if definition.viewsMode == "Covey" {
                let important = task.priority > 0 && task.priority <= 4
                let urgent = task.due.map { $0 <= urgentCutoff } ?? false
                group = important ? (urgent ? "Do First" : "Schedule") : (urgent ? "Delegate" : "Eliminate")
            } else {
                group = columns.first { column in
                    task.categories.contains { $0.caseInsensitiveCompare(column) == .orderedSame }
                } ?? "Untagged"
            }
            return DesktopBackgroundRow(
                title: task.summary,
                subtitle: task.due?.formatted(date: .abbreviated, time: .shortened) ?? "Task",
                group: group,
                kind: 1,
                overdue: (task.due ?? .distantFuture) < Date()
            )
        }
        for issue in visibleIssues {
            let group: String
            if definition.viewsMode == "Covey" {
                let labels = Set(issue.labels.map { $0.lowercased() })
                group = labels.contains("do") ? "Do First" :
                    labels.contains("delay") ? "Schedule" :
                    labels.contains("delegate") ? "Delegate" :
                    labels.contains("eliminate") ? "Eliminate" : "Untagged"
            } else {
                group = columns.first { column in
                    issue.labels.contains { $0.caseInsensitiveCompare(column) == .orderedSame }
                } ?? "Untagged"
            }
            if definition.viewsMode != "Covey" || group != "Untagged" {
                rows.append(
                    DesktopBackgroundRow(
                        title: issue.title,
                        subtitle: issue.repository,
                        group: group,
                        kind: 2,
                        overdue: false
                    )
                )
            }
        }
        let groups = definition.viewsMode == "Covey"
            ? ["Do First", "Schedule", "Delegate", "Eliminate"]
            : ["Untagged"] + columns
        return DesktopBackgroundContent(
            title: "VIEWS",
            filters: "\(definition.viewsType) · \(definition.viewsDate)",
            mode: definition.viewsMode,
            groups: groups,
            rows: rows
        )
    }

    private func matchesDate(_ date: Date?, filter: String) -> Bool {
        switch filter {
        case "Today": date.map { Calendar.current.isDateInToday($0) } == true
        case "Tomorrow": date.map { Calendar.current.isDateInTomorrow($0) } == true
        case "This week": date.flatMap { Calendar.current.dateInterval(of: .weekOfYear, for: Date())?.contains($0) } == true
        default: true
        }
    }

    private func backgroundDirectory() throws -> URL {
        let directory = try FileManager.default.url(
            for: .picturesDirectory,
            in: .userDomainMask,
            appropriateFor: nil,
            create: true
        )
        .appendingPathComponent("Cross-Dashboard", isDirectory: true)
        .appendingPathComponent("Backgrounds", isDirectory: true)
        try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)
        return directory
    }

    private func apply(directory: URL) throws {
        let dark = UserDefaults.standard.string(forKey: "AppleInterfaceStyle") == "Dark"
        for screen in NSScreen.screens {
            let url = directory.appendingPathComponent(
                "\(screenID(screen))_\(dark ? "dark" : "light").png"
            )
            guard FileManager.default.fileExists(atPath: url.path) else { continue }
            var options = NSWorkspace.shared.desktopImageOptions(for: screen) ?? [:]
            options[.imageScaling] = NSImageScaling.scaleProportionallyUpOrDown.rawValue
            options[.allowClipping] = true
            try NSWorkspace.shared.setDesktopImageURL(url, for: screen, options: options)
        }
    }

    private func screenID(_ screen: NSScreen) -> String {
        (screen.deviceDescription[NSDeviceDescriptionKey("NSScreenNumber")] as? NSNumber)?.stringValue ?? "main"
    }
}

private enum InboxBackgroundItem {
    case event(CalendarEvent)
    case task(CalDavTask)
    case issue(GiteaIssue)

    var title: String {
        switch self {
        case .event(let value): value.summary
        case .task(let value): value.summary
        case .issue(let value): value.title
        }
    }

    var subtitle: String {
        switch self {
        case .event(let value): value.start.formatted(date: .abbreviated, time: .shortened)
        case .task(let value): value.due?.formatted(date: .abbreviated, time: .shortened) ?? "No due date"
        case .issue(let value): value.repository
        }
    }

    var date: Date? {
        switch self {
        case .event(let value): value.start
        case .task(let value): value.due
        case .issue(let value): value.milestoneDueOn
        }
    }

    var kind: Int {
        switch self {
        case .event: 0
        case .task: 1
        case .issue: 2
        }
    }

    var overdue: Bool {
        if case .task(let value) = self { return (value.due ?? .distantFuture) < Date() }
        return false
    }

    var estimatedMinutes: Int {
        switch self {
        case .event(let value): max(0, Int(value.end.timeIntervalSince(value.start) / 60))
        case .task(let value): estimate((value.categories + [value.description ?? ""]).joined(separator: " "))
        case .issue(let value): estimate(value.body + " " + value.labels.map { "#\($0)" }.joined(separator: " "))
        }
    }

    private func estimate(_ text: String) -> Int {
        text.split(whereSeparator: \.isWhitespace).reduce(0) { total, token in
            guard let match = String(token).wholeMatch(of: /#?(\d+)(h|m)/) else { return total }
            let value = Int(match.1) ?? 0
            return total + (match.2 == "h" ? value * 60 : value)
        }
    }
}
