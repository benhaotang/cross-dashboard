import Foundation
import SwiftData

// ─── SwiftData @Model classes ─────────────────────────────────────────────────
// Each @Model mirrors an Android Room entity and provides toDomain() / init(from:) mappers.

@Model
public final class EventModel {
    @Attribute(.unique) public var uid: String
    public var summary: String
    public var startEpoch: Double        // Date.timeIntervalSince1970
    public var endEpoch: Double
    public var descriptionText: String?
    public var location: String?
    public var calendarHref: String?
    public var etag: String?
    public var href: String?

    public init(from event: CalendarEvent) {
        uid = event.uid
        summary = event.summary
        startEpoch = event.start.timeIntervalSince1970
        endEpoch = event.end.timeIntervalSince1970
        descriptionText = event.description
        location = event.location
        calendarHref = event.calendarHref
        etag = event.etag
        href = event.href
    }

    public func toDomain() -> CalendarEvent {
        CalendarEvent(
            uid: uid,
            summary: summary,
            start: Date(timeIntervalSince1970: startEpoch),
            end: Date(timeIntervalSince1970: endEpoch),
            description: descriptionText,
            location: location,
            calendarHref: calendarHref,
            etag: etag,
            href: href
        )
    }
}

@Model
public final class TaskModel {
    @Attribute(.unique) public var uid: String
    public var summary: String
    public var descriptionText: String?
    public var statusRaw: String
    public var priority: Int
    public var percentComplete: Int
    public var dueEpoch: Double?
    public var dtstartEpoch: Double?
    public var completedEpoch: Double?
    public var createdEpoch: Double
    public var lastModifiedEpoch: Double
    public var categoriesJSON: String    // JSON array string
    public var location: String?
    public var parentUid: String?
    public var calendarHref: String?
    public var etag: String?
    public var href: String?

    public init(from task: CalDavTask) {
        uid = task.uid
        summary = task.summary
        descriptionText = task.description
        statusRaw = task.status.rawValue
        priority = task.priority
        percentComplete = task.percentComplete
        dueEpoch = task.due?.timeIntervalSince1970
        dtstartEpoch = task.dtstart?.timeIntervalSince1970
        completedEpoch = task.completed?.timeIntervalSince1970
        createdEpoch = task.created.timeIntervalSince1970
        lastModifiedEpoch = task.lastModified.timeIntervalSince1970
        categoriesJSON = (try? String(data: JSONEncoder().encode(task.categories), encoding: .utf8)) ?? "[]"
        location = task.location
        parentUid = task.parentUid
        calendarHref = task.calendarHref
        etag = task.etag
        href = task.href
    }

    public func toDomain() -> CalDavTask {
        let cats = (try? JSONDecoder().decode([String].self, from: Data(categoriesJSON.utf8))) ?? []
        return CalDavTask(
            uid: uid,
            summary: summary,
            description: descriptionText,
            status: TaskStatus(rawValue: statusRaw) ?? .needsAction,
            priority: priority,
            percentComplete: percentComplete,
            due: dueEpoch.map { Date(timeIntervalSince1970: $0) },
            dtstart: dtstartEpoch.map { Date(timeIntervalSince1970: $0) },
            completed: completedEpoch.map { Date(timeIntervalSince1970: $0) },
            created: Date(timeIntervalSince1970: createdEpoch),
            lastModified: Date(timeIntervalSince1970: lastModifiedEpoch),
            categories: cats,
            location: location,
            parentUid: parentUid,
            calendarHref: calendarHref,
            etag: etag,
            href: href
        )
    }
}

@Model
public final class NoteModel {
    @Attribute(.unique) public var uid: String
    public var summary: String
    public var body: String
    public var categoriesJSON: String
    public var createdEpoch: Double
    public var lastModifiedEpoch: Double
    public var calendarHref: String?
    public var etag: String?
    public var href: String?

    public init(from note: Note) {
        uid = note.uid
        summary = note.summary
        body = note.body
        categoriesJSON = (try? String(data: JSONEncoder().encode(note.categories), encoding: .utf8)) ?? "[]"
        createdEpoch = note.created.timeIntervalSince1970
        lastModifiedEpoch = note.lastModified.timeIntervalSince1970
        calendarHref = note.calendarHref
        etag = note.etag
        href = note.href
    }

    public func toDomain() -> Note {
        let cats = (try? JSONDecoder().decode([String].self, from: Data(categoriesJSON.utf8))) ?? []
        return Note(
            uid: uid,
            summary: summary,
            body: body,
            categories: cats,
            created: Date(timeIntervalSince1970: createdEpoch),
            lastModified: Date(timeIntervalSince1970: lastModifiedEpoch),
            calendarHref: calendarHref,
            etag: etag,
            href: href
        )
    }
}

@Model
public final class IssueModel {
    @Attribute(.unique) public var issueId: Int64
    public var number: Int
    public var title: String
    public var body: String
    public var state: String
    public var labelsJSON: String      // JSON string[]
    public var assigneesJSON: String
    public var createdAtEpoch: Double
    public var updatedAtEpoch: Double
    public var repository: String
    public var htmlUrl: String

    public init(from issue: GiteaIssue) {
        issueId = issue.id
        number = issue.number
        title = issue.title
        body = issue.body
        state = issue.state
        labelsJSON = (try? String(data: JSONEncoder().encode(issue.labels), encoding: .utf8)) ?? "[]"
        assigneesJSON = (try? String(data: JSONEncoder().encode(issue.assignees), encoding: .utf8)) ?? "[]"
        createdAtEpoch = issue.createdAt.timeIntervalSince1970
        updatedAtEpoch = issue.updatedAt.timeIntervalSince1970
        repository = issue.repository
        htmlUrl = issue.htmlUrl
    }

    public func toDomain() -> GiteaIssue {
        let labels = (try? JSONDecoder().decode([String].self, from: Data(labelsJSON.utf8))) ?? []
        let assignees = (try? JSONDecoder().decode([String].self, from: Data(assigneesJSON.utf8))) ?? []
        return GiteaIssue(
            id: issueId,
            number: number,
            title: title,
            body: body,
            state: state,
            labels: labels,
            assignees: assignees,
            createdAt: Date(timeIntervalSince1970: createdAtEpoch),
            updatedAt: Date(timeIntervalSince1970: updatedAtEpoch),
            repository: repository,
            htmlUrl: htmlUrl
        )
    }
}

@Model
public final class StatsModel {
    @Attribute(.unique) public var date: String   // "yyyy-MM-dd"
    public var tasksCompleted: Int
    public var pomodoroSessions: Int
    public var issuesClosed: Int

    public init(date: String) {
        self.date = date
        tasksCompleted = 0
        pomodoroSessions = 0
        issuesClosed = 0
    }

    public convenience init(from stats: DailyStats) {
        self.init(date: stats.date)
        tasksCompleted = stats.tasksCompleted
        pomodoroSessions = stats.pomodoroSessions
        issuesClosed = stats.issuesClosed
    }

    public func toDomain() -> DailyStats {
        DailyStats(
            date: date,
            tasksCompleted: tasksCompleted,
            pomodoroSessions: pomodoroSessions,
            issuesClosed: issuesClosed
        )
    }
}

// ─── PersistenceController ────────────────────────────────────────────────────

@MainActor
public final class PersistenceController {
    public static let shared = PersistenceController()

    public let container: ModelContainer

    /// All SwiftData model types registered in the schema.
    public static let schema = Schema([
        EventModel.self,
        TaskModel.self,
        NoteModel.self,
        IssueModel.self,
        StatsModel.self,
    ])

    /// Returns the SwiftData store URL inside the shared App Group container so
    /// the widget extension can read the same database as the main app.
    /// Falls back to the default location if the group container is unavailable.
    public static func groupContainerURL(appGroupID: String = "group.com.crossdashboard") -> URL? {
        FileManager.default
            .containerURL(forSecurityApplicationGroupIdentifier: appGroupID)?
            .appendingPathComponent("CrossDashboard.sqlite")
    }

    private init() {
        do {
            let config: ModelConfiguration
            if let storeURL = PersistenceController.groupContainerURL() {
                config = ModelConfiguration(schema: PersistenceController.schema, url: storeURL)
            } else {
                config = ModelConfiguration(schema: PersistenceController.schema, isStoredInMemoryOnly: false)
            }
            container = try ModelContainer(
                for: PersistenceController.schema,
                configurations: [config]
            )
        } catch {
            fatalError("PersistenceController: failed to create ModelContainer — \(error)")
        }
    }

    /// Creates a ModelContainer stored entirely in memory. Used in tests and SwiftUI Previews.
    public static func preview() -> PersistenceController {
        let instance = PersistenceController(inMemory: true)
        return instance
    }

    private init(inMemory: Bool) {
        do {
            let config = ModelConfiguration(
                schema: PersistenceController.schema,
                isStoredInMemoryOnly: inMemory
            )
            container = try ModelContainer(
                for: PersistenceController.schema,
                configurations: [config]
            )
        } catch {
            fatalError("PersistenceController (preview): failed — \(error)")
        }
    }
}
