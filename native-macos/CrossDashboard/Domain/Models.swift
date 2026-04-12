import Foundation

// ─── CalDAV ───────────────────────────────────────────────────────────────────

struct CalDavCalendar: Codable, Identifiable, Hashable {
    var id: String { href }
    let href: String
    let displayName: String
    let color: String?        // normalized #RRGGBB
    let ctag: String?
    let components: [String]  // VEVENT, VTODO, VJOURNAL
}

struct CalendarEvent: Codable, Identifiable, Hashable {
    var id: String { uid }
    let uid: String
    let summary: String
    let start: Date
    let end: Date
    let description: String?
    let location: String?
    let calendarHref: String?
    let etag: String?
    let href: String?
}

enum TaskStatus: String, Codable, CaseIterable {
    case needsAction = "NEEDS-ACTION"
    case inProcess   = "IN-PROCESS"
    case completed   = "COMPLETED"
    case cancelled   = "CANCELLED"

    static func fromIcal(_ value: String) -> TaskStatus {
        allCases.first { $0.rawValue.lowercased() == value.lowercased() } ?? .needsAction
    }
}

struct CalDavTask: Codable, Identifiable, Hashable {
    var id: String { uid }
    let uid: String
    let summary: String
    let description: String?
    let status: TaskStatus
    let priority: Int           // 0=none, 1-4=high, 5=med, 6-9=low
    let percentComplete: Int
    let due: Date?
    let dtstart: Date?
    let completed: Date?
    let created: Date
    let lastModified: Date
    let categories: [String]
    let location: String?
    let parentUid: String?
    let calendarHref: String?
    let etag: String?
    let href: String?

    init(
        uid: String = UUID().uuidString,
        summary: String,
        description: String? = nil,
        status: TaskStatus = .needsAction,
        priority: Int = 0,
        percentComplete: Int = 0,
        due: Date? = nil,
        dtstart: Date? = nil,
        completed: Date? = nil,
        created: Date = Date(),
        lastModified: Date = Date(),
        categories: [String] = [],
        location: String? = nil,
        parentUid: String? = nil,
        calendarHref: String? = nil,
        etag: String? = nil,
        href: String? = nil
    ) {
        self.uid = uid
        self.summary = summary
        self.description = description
        self.status = status
        self.priority = priority
        self.percentComplete = percentComplete
        self.due = due
        self.dtstart = dtstart
        self.completed = completed
        self.created = created
        self.lastModified = lastModified
        self.categories = categories
        self.location = location
        self.parentUid = parentUid
        self.calendarHref = calendarHref
        self.etag = etag
        self.href = href
    }
}

struct Note: Codable, Identifiable, Hashable {
    var id: String { uid }
    let uid: String
    let summary: String
    let body: String
    let categories: [String]
    let created: Date
    let lastModified: Date
    let calendarHref: String?
    let etag: String?
    let href: String?

    init(
        uid: String = UUID().uuidString,
        summary: String,
        body: String = "",
        categories: [String] = [],
        created: Date = Date(),
        lastModified: Date = Date(),
        calendarHref: String? = nil,
        etag: String? = nil,
        href: String? = nil
    ) {
        self.uid = uid
        self.summary = summary
        self.body = body
        self.categories = categories
        self.created = created
        self.lastModified = lastModified
        self.calendarHref = calendarHref
        self.etag = etag
        self.href = href
    }
}

// ─── Gitea ────────────────────────────────────────────────────────────────────

struct GiteaIssue: Codable, Identifiable, Hashable {
    let id: Int64
    let number: Int
    let title: String
    let body: String
    let state: String           // "open" | "closed"
    let labels: [String]
    let assignees: [String]
    let createdAt: Date
    let updatedAt: Date
    let repository: String      // "owner/repo"
    let htmlUrl: String
}

struct GiteaComment: Codable, Identifiable, Hashable {
    let id: Int64
    let body: String
    let user: String
    let createdAt: Date
}

struct GiteaLabel: Codable, Identifiable, Hashable {
    let id: Int64
    let name: String
    let color: String           // hex without #
}

struct GiteaMilestone: Codable, Identifiable, Hashable {
    let id: Int64
    let title: String
    let dueOn: Date?
    let openIssues: Int
    let closedIssues: Int
}

struct GiteaAttachment: Codable, Identifiable, Hashable {
    let id: Int64
    let name: String
    let downloadUrl: String
    let size: Int64
    let uuid: String
}

// ─── Stats ────────────────────────────────────────────────────────────────────

struct DailyStats: Codable, Identifiable, Hashable {
    var id: String { date }
    let date: String            // ISO-8601 "yyyy-MM-dd"
    let tasksCompleted: Int
    let pomodoroSessions: Int
    let issuesClosed: Int

    init(date: String, tasksCompleted: Int = 0, pomodoroSessions: Int = 0, issuesClosed: Int = 0) {
        self.date = date
        self.tasksCompleted = tasksCompleted
        self.pomodoroSessions = pomodoroSessions
        self.issuesClosed = issuesClosed
    }
}

// ─── Auth ─────────────────────────────────────────────────────────────────────

enum CalDavAuthMethod: String, Codable, CaseIterable {
    case loginFlowV2 = "LOGIN_FLOW_V2"
    case manual      = "MANUAL"
}

struct CalDavCredentials: Codable {
    let authMethod: CalDavAuthMethod
    let serverUrl: String
    let username: String
    let password: String?
}

// ─── Inbox aggregation ────────────────────────────────────────────────────────

enum InboxItem: Identifiable {
    case event(CalendarEvent, durationMinutes: Int)
    case task(CalDavTask, estimatedMinutes: Int?)
    case issue(GiteaIssue, estimatedMinutes: Int?)
    case milestone(GiteaMilestone)

    var id: String {
        switch self {
        case .event(let e, _):     return "event-\(e.uid)"
        case .task(let t, _):      return "task-\(t.uid)"
        case .issue(let i, _):     return "issue-\(i.id)"
        case .milestone(let m):    return "milestone-\(m.id)"
        }
    }
}

// ─── Pomodoro ─────────────────────────────────────────────────────────────────

struct PomodoroSettings: Codable, Equatable {
    var workMinutes: Int = 25
    var shortBreakMinutes: Int = 5
    var longBreakMinutes: Int = 15
    var sessionsUntilLongBreak: Int = 4
}

enum PomodoroPhase: String, Codable, CaseIterable {
    case work        = "WORK"
    case shortBreak  = "SHORT_BREAK"
    case longBreak   = "LONG_BREAK"

    var label: String {
        switch self {
        case .work:       return "Focus"
        case .shortBreak: return "Short break"
        case .longBreak:  return "Long break"
        }
    }
}

struct PomodoroState: Codable, Equatable {
    var phase: PomodoroPhase = .work
    var secondsLeft: Int = 25 * 60
    var running: Bool = false
    var currentSession: Int = 1
    var completedSessions: Int = 0
    var itemTitle: String = ""
    var active: Bool = false
    var settings: PomodoroSettings = PomodoroSettings()
}

// ─── Task input parsing ───────────────────────────────────────────────────────

struct TaskDefaults: Codable, Equatable {
    var morningHour: Int = 8
    var afternoonHour: Int = 13
    var nightHour: Int = 21
    var defaultHour: Int = 10
}

struct ParsedTask {
    let summary: String
    let priority: Int           // 0, 1 (high), 5 (med), 9 (low)
    let categories: [String]
    let due: Date?
}

// ─── App preferences ─────────────────────────────────────────────────────────

enum ThemePreference: String, Codable, CaseIterable {
    case system = "SYSTEM"
    case light  = "LIGHT"
    case dark   = "DARK"
}

let allScreens: [String] = ["Dashboard", "Inbox", "Events", "Tasks", "Notes", "Issues", "Views"]
let defaultKanbanColumns: [String] = ["backlog", "planned", "inprogress", "done"]

struct AppSettings: Codable, Equatable {
    var theme: ThemePreference = .system
    var visibleScreens: [String] = allScreens
    var kanbanColumns: [String] = defaultKanbanColumns
    var pomodoroSettings: PomodoroSettings = PomodoroSettings()
    var taskDefaults: TaskDefaults = TaskDefaults()
    var notificationsEnabled: Bool = true
    var notificationMinutesBefore: Int = 15
    var biometricLockEnabled: Bool = false
    var showPomodoroInMenuBar: Bool = true
}
