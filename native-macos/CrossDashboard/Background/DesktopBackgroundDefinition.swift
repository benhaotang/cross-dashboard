import Foundation

enum DesktopBackgroundSource: String, Codable { case inbox, views }

struct DesktopBackgroundDefinition: Codable, Equatable {
    var enabled = true
    var source: DesktopBackgroundSource
    var inboxType = "All"
    var inboxDate = "All"
    var searchQuery: String?
    var viewsType = "All"
    var viewsDate = "All"
    var viewsMode = "Kanban"
    var capturedAt = Date()

    var summary: String {
        switch source {
        case .inbox:
            "Inbox · \(inboxType) · \(inboxDate)" + (searchQuery.map { " · Search “\($0)”" } ?? "")
        case .views: "Views · \(viewsMode) · \(viewsType) · \(viewsDate)"
        }
    }
}

struct DesktopBackgroundRow {
    let title: String
    let subtitle: String
    let group: String?
    let kind: Int
    let overdue: Bool
}

struct DesktopBackgroundContent {
    let title: String
    let filters: String
    let mode: String?
    let groups: [String]
    let rows: [DesktopBackgroundRow]
    let totalMinutes: Int
    let refreshedAt: Date

    init(title: String, filters: String, mode: String?, groups: [String] = [], rows: [DesktopBackgroundRow], totalMinutes: Int = 0) {
        self.title = title
        self.filters = filters
        self.mode = mode
        self.groups = groups
        self.rows = rows
        self.totalMinutes = totalMinutes
        self.refreshedAt = Date()
    }
}
