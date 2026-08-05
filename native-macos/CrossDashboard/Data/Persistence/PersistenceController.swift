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
    public var milestoneId: Int64? = nil
    public var milestoneTitle: String? = nil
    public var milestoneDueOnEpoch: Double? = nil

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
        milestoneId = issue.milestoneId
        milestoneTitle = issue.milestoneTitle
        milestoneDueOnEpoch = issue.milestoneDueOn?.timeIntervalSince1970
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
            htmlUrl: htmlUrl,
            milestoneId: milestoneId,
            milestoneTitle: milestoneTitle,
            milestoneDueOn: milestoneDueOnEpoch.map { Date(timeIntervalSince1970: $0) }
        )
    }
}

@Model
public final class MemosModel {
    @Attribute(.unique) public var name: String    // "memos/{id}"
    public var stateRaw: String
    public var content: String
    public var visibilityRaw: String
    public var tagsJSON: String                    // JSON string[]
    public var pinned: Bool
    public var attachmentsJSON: String             // JSON [MemosAttachment]
    public var propertyHasLink: Bool
    public var propertyHasTaskList: Bool
    public var propertyHasIncompleteTasks: Bool
    public var propertyTitle: String
    public var snippet: String
    public var createTimeEpoch: Double
    public var displayTimeEpoch: Double
    public var updateTimeEpoch: Double

    public init(from memo: MemosMemo) {
        name                    = memo.name
        stateRaw                = memo.state.rawValue
        content                 = memo.content
        visibilityRaw           = memo.visibility.rawValue
        tagsJSON                = (try? String(data: JSONEncoder().encode(memo.tags), encoding: .utf8)) ?? "[]"
        pinned                  = memo.pinned
        attachmentsJSON         = (try? String(data: JSONEncoder().encode(memo.attachments), encoding: .utf8)) ?? "[]"
        propertyHasLink         = memo.property.hasLink
        propertyHasTaskList     = memo.property.hasTaskList
        propertyHasIncompleteTasks = memo.property.hasIncompleteTasks
        propertyTitle           = memo.property.title
        snippet                 = memo.snippet
        createTimeEpoch         = memo.createTime.timeIntervalSince1970
        displayTimeEpoch        = memo.displayTime.timeIntervalSince1970
        updateTimeEpoch         = memo.updateTime.timeIntervalSince1970
    }

    public func toDomain() -> MemosMemo {
        let tags        = (try? JSONDecoder().decode([String].self, from: Data(tagsJSON.utf8))) ?? []
        let attachments = (try? JSONDecoder().decode([MemosAttachment].self, from: Data(attachmentsJSON.utf8))) ?? []
        return MemosMemo(
            name: name,
            state: MemoState(rawValue: stateRaw) ?? .normal,
            content: content,
            visibility: MemoVisibility(rawValue: visibilityRaw) ?? .private,
            tags: tags,
            pinned: pinned,
            attachments: attachments,
            property: MemoProperty(
                hasLink: propertyHasLink,
                hasTaskList: propertyHasTaskList,
                hasIncompleteTasks: propertyHasIncompleteTasks,
                title: propertyTitle
            ),
            snippet: snippet,
            createTime:  Date(timeIntervalSince1970: createTimeEpoch),
            displayTime: Date(timeIntervalSince1970: displayTimeEpoch),
            updateTime:  Date(timeIntervalSince1970: updateTimeEpoch)
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
    public nonisolated static let schema = Schema([
        EventModel.self,
        TaskModel.self,
        NoteModel.self,
        IssueModel.self,
        StatsModel.self,
        MemosModel.self,
    ])

    /// Returns the SwiftData store URL inside the shared App Group container so
    /// the widget extension can read the same database as the main app.
    /// Falls back to the default location if the group container is unavailable.
    public nonisolated static func groupContainerURL(appGroupID: String = "group.com.crossdashboard") -> URL? {
        FileManager.default
            .containerURL(forSecurityApplicationGroupIdentifier: appGroupID)?
            .appendingPathComponent("CrossDashboard.sqlite")
    }

    private init() {
        do {
            // Use the App Group container so the widget extension reads the same store.
            // groupContainer: .identifier(...) is the correct SwiftData API for this;
            // the URL-based ModelConfiguration init is not available on macOS 15.
            let config = ModelConfiguration(
                schema: PersistenceController.schema,
                groupContainer: .identifier("group.com.crossdashboard")
            )
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
