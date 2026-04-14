import Foundation

// ─── CalDAV ───────────────────────────────────────────────────────────────────

public struct CalDavCalendar: Codable, Identifiable, Hashable, Sendable {
    public var id: String { href }
    public let href: String
    public let displayName: String
    public let color: String?        // normalized #RRGGBB
    public let ctag: String?
    public let components: [String]  // VEVENT, VTODO, VJOURNAL

    public init(href: String, displayName: String, color: String? = nil, ctag: String? = nil, components: [String] = []) {
        self.href = href
        self.displayName = displayName
        self.color = color
        self.ctag = ctag
        self.components = components
    }
}

public struct CalendarEvent: Codable, Identifiable, Hashable, Sendable {
    public var id: String { uid }
    public let uid: String
    public let summary: String
    public let start: Date
    public let end: Date
    public let description: String?
    public let location: String?
    public let calendarHref: String?
    public let etag: String?
    public let href: String?

    public init(uid: String, summary: String, start: Date, end: Date,
                description: String? = nil, location: String? = nil,
                calendarHref: String? = nil, etag: String? = nil, href: String? = nil) {
        self.uid = uid
        self.summary = summary
        self.start = start
        self.end = end
        self.description = description
        self.location = location
        self.calendarHref = calendarHref
        self.etag = etag
        self.href = href
    }
}

public enum TaskStatus: String, Codable, CaseIterable, Sendable {
    case needsAction = "NEEDS-ACTION"
    case inProcess   = "IN-PROCESS"
    case completed   = "COMPLETED"
    case cancelled   = "CANCELLED"

    public static func fromIcal(_ value: String) -> TaskStatus {
        allCases.first { $0.rawValue.lowercased() == value.lowercased() } ?? .needsAction
    }
}

public struct CalDavTask: Codable, Identifiable, Hashable, Sendable {
    public var id: String { uid }
    public let uid: String
    public let summary: String
    public let description: String?
    public let status: TaskStatus
    public let priority: Int           // 0=none, 1-4=high, 5=med, 6-9=low
    public let percentComplete: Int
    public let due: Date?
    public let dtstart: Date?
    public let completed: Date?
    public let created: Date
    public let lastModified: Date
    public let categories: [String]
    public let location: String?
    public let parentUid: String?
    public let calendarHref: String?
    public let etag: String?
    public let href: String?

    public init(
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

public struct Note: Codable, Identifiable, Hashable, Sendable {
    public var id: String { uid }
    public let uid: String
    public let summary: String
    public let body: String
    public let categories: [String]
    public let created: Date
    public let lastModified: Date
    public let calendarHref: String?
    public let etag: String?
    public let href: String?

    public init(
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

public struct GiteaIssue: Codable, Identifiable, Hashable, Sendable {
    public let id: Int64
    public let number: Int
    public let title: String
    public let body: String
    public let state: String           // "open" | "closed"
    public let labels: [String]
    public let assignees: [String]
    public let createdAt: Date
    public let updatedAt: Date
    public let repository: String      // "owner/repo"
    public let htmlUrl: String

    public init(id: Int64, number: Int, title: String, body: String, state: String,
                labels: [String] = [], assignees: [String] = [],
                createdAt: Date, updatedAt: Date, repository: String, htmlUrl: String) {
        self.id = id
        self.number = number
        self.title = title
        self.body = body
        self.state = state
        self.labels = labels
        self.assignees = assignees
        self.createdAt = createdAt
        self.updatedAt = updatedAt
        self.repository = repository
        self.htmlUrl = htmlUrl
    }
}

public struct GiteaComment: Codable, Identifiable, Hashable, Sendable {
    public let id: Int64
    public let body: String
    public let user: String
    public let createdAt: Date

    public init(id: Int64, body: String, user: String, createdAt: Date) {
        self.id = id
        self.body = body
        self.user = user
        self.createdAt = createdAt
    }
}

public struct GiteaLabel: Codable, Identifiable, Hashable, Sendable {
    public let id: Int64
    public let name: String
    public let color: String           // hex without #

    public init(id: Int64, name: String, color: String) {
        self.id = id
        self.name = name
        self.color = color
    }
}

public struct GiteaMilestone: Codable, Identifiable, Hashable, Sendable {
    public let id: Int64
    public let title: String
    public let dueOn: Date?
    public let openIssues: Int
    public let closedIssues: Int

    public init(id: Int64, title: String, dueOn: Date? = nil, openIssues: Int = 0, closedIssues: Int = 0) {
        self.id = id
        self.title = title
        self.dueOn = dueOn
        self.openIssues = openIssues
        self.closedIssues = closedIssues
    }
}

public struct GiteaAttachment: Codable, Identifiable, Hashable, Sendable {
    public let id: Int64
    public let name: String
    public let downloadUrl: String
    public let size: Int64
    public let uuid: String

    public init(id: Int64, name: String, downloadUrl: String, size: Int64, uuid: String) {
        self.id = id
        self.name = name
        self.downloadUrl = downloadUrl
        self.size = size
        self.uuid = uuid
    }
}

// ─── Stats ────────────────────────────────────────────────────────────────────

public struct DailyStats: Codable, Identifiable, Hashable, Sendable {
    public var id: String { date }
    public let date: String            // ISO-8601 "yyyy-MM-dd"
    public let tasksCompleted: Int
    public let pomodoroSessions: Int
    public let issuesClosed: Int

    public init(date: String, tasksCompleted: Int = 0, pomodoroSessions: Int = 0, issuesClosed: Int = 0) {
        self.date = date
        self.tasksCompleted = tasksCompleted
        self.pomodoroSessions = pomodoroSessions
        self.issuesClosed = issuesClosed
    }
}

// ─── Auth ─────────────────────────────────────────────────────────────────────

public enum CalDavAuthMethod: String, Codable, CaseIterable, Sendable {
    case loginFlowV2 = "LOGIN_FLOW_V2"
    case manual      = "MANUAL"
}

public struct CalDavCredentials: Codable, Sendable {
    public let authMethod: CalDavAuthMethod
    public let serverUrl: String
    public let username: String
    public let password: String?

    public init(authMethod: CalDavAuthMethod, serverUrl: String, username: String, password: String? = nil) {
        self.authMethod = authMethod
        self.serverUrl = serverUrl
        self.username = username
        self.password = password
    }
}

// ─── Inbox aggregation ────────────────────────────────────────────────────────

public enum InboxItem: Identifiable, Sendable {
    case event(CalendarEvent, durationMinutes: Int)
    case task(CalDavTask, estimatedMinutes: Int?)
    case issue(GiteaIssue, estimatedMinutes: Int?)
    case milestone(GiteaMilestone)

    public var id: String {
        switch self {
        case .event(let e, _):     return "event-\(e.uid)"
        case .task(let t, _):      return "task-\(t.uid)"
        case .issue(let i, _):     return "issue-\(i.id)"
        case .milestone(let m):    return "milestone-\(m.id)"
        }
    }
}

// ─── Pomodoro ─────────────────────────────────────────────────────────────────

public struct PomodoroSettings: Codable, Equatable, Sendable {
    public var workMinutes: Int = 25
    public var shortBreakMinutes: Int = 5
    public var longBreakMinutes: Int = 15
    public var sessionsUntilLongBreak: Int = 4

    public init(workMinutes: Int = 25, shortBreakMinutes: Int = 5,
                longBreakMinutes: Int = 15, sessionsUntilLongBreak: Int = 4) {
        self.workMinutes = workMinutes
        self.shortBreakMinutes = shortBreakMinutes
        self.longBreakMinutes = longBreakMinutes
        self.sessionsUntilLongBreak = sessionsUntilLongBreak
    }
}

public enum PomodoroPhase: String, Codable, CaseIterable, Sendable {
    case work        = "WORK"
    case shortBreak  = "SHORT_BREAK"
    case longBreak   = "LONG_BREAK"

    public var label: String {
        switch self {
        case .work:       return "Focus"
        case .shortBreak: return "Short break"
        case .longBreak:  return "Long break"
        }
    }
}

public struct PomodoroState: Codable, Equatable, Sendable {
    public var phase: PomodoroPhase = .work
    public var secondsLeft: Int = 25 * 60
    public var running: Bool = false
    public var currentSession: Int = 1
    public var completedSessions: Int = 0
    public var itemTitle: String = ""
    public var active: Bool = false
    public var settings: PomodoroSettings = PomodoroSettings()

    public init(phase: PomodoroPhase = .work, secondsLeft: Int = 25 * 60,
                running: Bool = false, currentSession: Int = 1,
                completedSessions: Int = 0, itemTitle: String = "",
                active: Bool = false, settings: PomodoroSettings = PomodoroSettings()) {
        self.phase = phase
        self.secondsLeft = secondsLeft
        self.running = running
        self.currentSession = currentSession
        self.completedSessions = completedSessions
        self.itemTitle = itemTitle
        self.active = active
        self.settings = settings
    }
}

// ─── Task input parsing ───────────────────────────────────────────────────────

public struct TaskDefaults: Codable, Equatable, Sendable {
    public var morningHour: Int = 8
    public var afternoonHour: Int = 13
    public var nightHour: Int = 21
    public var defaultHour: Int = 10

    public init(morningHour: Int = 8, afternoonHour: Int = 13,
                nightHour: Int = 21, defaultHour: Int = 10) {
        self.morningHour = morningHour
        self.afternoonHour = afternoonHour
        self.nightHour = nightHour
        self.defaultHour = defaultHour
    }
}

public struct ParsedTask: Sendable {
    public let summary: String
    public let priority: Int           // 0, 1 (high), 5 (med), 9 (low)
    public let categories: [String]
    public let due: Date?

    public init(summary: String, priority: Int = 0, categories: [String] = [], due: Date? = nil) {
        self.summary = summary
        self.priority = priority
        self.categories = categories
        self.due = due
    }
}

// ─── App preferences ─────────────────────────────────────────────────────────

public enum ThemePreference: String, Codable, CaseIterable, Sendable {
    case system = "SYSTEM"
    case light  = "LIGHT"
    case dark   = "DARK"
}

// ─── Memos (usememos.com) ─────────────────────────────────────────────────────

public enum MemoState: String, Codable, CaseIterable, Sendable {
    case normal   = "NORMAL"
    case archived = "ARCHIVED"
}

public enum MemoVisibility: String, Codable, CaseIterable, Sendable {
    case `private`  = "PRIVATE"
    case protected_ = "PROTECTED"
    case `public`   = "PUBLIC"
}

public struct MemoProperty: Codable, Equatable, Hashable, Sendable {
    public let hasLink: Bool
    public let hasTaskList: Bool
    public let hasIncompleteTasks: Bool
    public let title: String

    public init(hasLink: Bool = false, hasTaskList: Bool = false,
                hasIncompleteTasks: Bool = false, title: String = "") {
        self.hasLink = hasLink
        self.hasTaskList = hasTaskList
        self.hasIncompleteTasks = hasIncompleteTasks
        self.title = title
    }
}

public struct MemosAttachment: Codable, Identifiable, Hashable, Sendable {
    public var id: String { name }
    public let name: String           // "attachments/{id}"
    public let filename: String
    public let externalLink: String
    public let type: String           // MIME type
    public let size: Int64
    public let memo: String           // "memos/{id}" back-reference

    public init(name: String, filename: String, externalLink: String,
                type: String, size: Int64, memo: String = "") {
        self.name = name
        self.filename = filename
        self.externalLink = externalLink
        self.type = type
        self.size = size
        self.memo = memo
    }
}

public struct MemosMemo: Codable, Identifiable, Hashable, Sendable {
    public var id: String { name }
    public let name: String           // "memos/{id}"
    public let state: MemoState
    public let content: String
    public let visibility: MemoVisibility
    public let tags: [String]
    public let pinned: Bool
    public let attachments: [MemosAttachment]
    public let property: MemoProperty
    public let snippet: String
    public let createTime: Date
    public let displayTime: Date
    public let updateTime: Date

    public init(name: String, state: MemoState = .normal, content: String,
                visibility: MemoVisibility = .private, tags: [String] = [],
                pinned: Bool = false, attachments: [MemosAttachment] = [],
                property: MemoProperty = MemoProperty(), snippet: String = "",
                createTime: Date = Date(), displayTime: Date = Date(),
                updateTime: Date = Date()) {
        self.name = name
        self.state = state
        self.content = content
        self.visibility = visibility
        self.tags = tags
        self.pinned = pinned
        self.attachments = attachments
        self.property = property
        self.snippet = snippet
        self.createTime = createTime
        self.displayTime = displayTime
        self.updateTime = updateTime
    }
}

public let allScreens: [String] = ["Dashboard", "Inbox", "Events", "Tasks", "Notes", "Issues", "Views", "Capture"]
public let defaultKanbanColumns: [String] = ["backlog", "planned", "inprogress", "done"]

public struct AppSettings: Codable, Equatable, Sendable {
    public var theme: ThemePreference = .system
    public var visibleScreens: [String] = allScreens
    public var kanbanColumns: [String] = defaultKanbanColumns
    public var pomodoroSettings: PomodoroSettings = PomodoroSettings()
    public var taskDefaults: TaskDefaults = TaskDefaults()
    public var notificationsEnabled: Bool = true
    public var notificationMinutesBefore: Int = 15
    public var biometricLockEnabled: Bool = false
    public var showPomodoroInMenuBar: Bool = true

    public init() {}
}
