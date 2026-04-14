package com.crossdashboard.app.domain.model

import java.time.Instant
import java.time.LocalDate
import kotlinx.serialization.Serializable

// ─── CalDAV ───────────────────────────────────────────────────────────────────

@Serializable
data class CalDavCalendar(
    val href: String,
    val displayName: String,
    val color: String? = null,       // normalized #RRGGBB
    val ctag: String? = null,
    val components: List<String> = emptyList(),  // VEVENT, VTODO, VJOURNAL
)

data class CalendarEvent(
    val uid: String,
    val summary: String,
    val start: Instant,
    val end: Instant,
    val description: String? = null,
    val location: String? = null,
    val calendarHref: String? = null,
    val etag: String? = null,
    val href: String? = null,  // resource URL for PUT/DELETE
)

enum class TaskStatus(val icalValue: String) {
    NEEDS_ACTION("NEEDS-ACTION"),
    IN_PROCESS("IN-PROCESS"),
    COMPLETED("COMPLETED"),
    CANCELLED("CANCELLED");

    companion object {
        fun fromIcal(value: String): TaskStatus =
            entries.firstOrNull { it.icalValue.equals(value, ignoreCase = true) }
                ?: NEEDS_ACTION
    }
}

data class CalDavTask(
    val uid: String,
    val summary: String,
    val description: String? = null,
    val status: TaskStatus = TaskStatus.NEEDS_ACTION,
    val priority: Int = 0,          // 0=none, 1-4=high, 5=med, 6-9=low
    val percentComplete: Int = 0,
    val due: Instant? = null,
    val dtstart: Instant? = null,
    val completed: Instant? = null,
    val created: Instant = Instant.now(),
    val lastModified: Instant = Instant.now(),
    val categories: List<String> = emptyList(),
    val location: String? = null,
    val parentUid: String? = null,
    val calendarHref: String? = null,
    val etag: String? = null,
    val href: String? = null,
)

data class Note(
    val uid: String,
    val summary: String,
    val body: String = "",
    val categories: List<String> = emptyList(),
    val created: Instant = Instant.now(),
    val lastModified: Instant = Instant.now(),
    val calendarHref: String? = null,
    val etag: String? = null,
    val href: String? = null,
)

// ─── Gitea ────────────────────────────────────────────────────────────────────

data class GiteaIssue(
    val id: Long,
    val number: Int,
    val title: String,
    val body: String,
    val state: String,          // "open" | "closed"
    val labels: List<String>,
    val assignees: List<String>,
    val createdAt: Instant,
    val updatedAt: Instant,
    val repository: String,     // "owner/repo"
    val htmlUrl: String,
)

data class GiteaComment(
    val id: Long,
    val body: String,
    val user: String,
    val createdAt: Instant,
)

data class GiteaLabel(
    val id: Long,
    val name: String,
    val color: String,          // hex without #
)

data class GiteaMilestone(
    val id: Long,
    val title: String,
    val dueOn: Instant?,
    val openIssues: Int,
    val closedIssues: Int,
)

data class GiteaAttachment(
    val id: Long,
    val name: String,
    val downloadUrl: String,
    val size: Long,
    val uuid: String,
)

// ─── Stats ────────────────────────────────────────────────────────────────────

data class DailyStats(
    val date: LocalDate,
    val tasksCompleted: Int = 0,
    val pomodoroSessions: Int = 0,
    val issuesClosed: Int = 0,
)

// ─── Auth ─────────────────────────────────────────────────────────────────────

enum class CalDavAuthMethod { NEXTCLOUD_SSO, LOGIN_FLOW_V2, MANUAL }

data class CalDavCredentials(
    val authMethod: CalDavAuthMethod,
    val serverUrl: String,
    val username: String,
    val password: String?,          // null when using SSO
    val ssoAccountName: String?,    // only when NEXTCLOUD_SSO
)

// ─── Inbox aggregation ────────────────────────────────────────────────────────

sealed class InboxItem {
    data class Event(val event: CalendarEvent, val durationMinutes: Int) : InboxItem()
    data class Task(val task: CalDavTask, val estimatedMinutes: Int?) : InboxItem()
    data class Issue(val issue: GiteaIssue, val estimatedMinutes: Int?) : InboxItem()
    data class Milestone(val milestone: GiteaMilestone) : InboxItem()
}

// ─── Pomodoro ─────────────────────────────────────────────────────────────────

data class PomodoroSettings(
    val workMinutes: Int = 25,
    val shortBreakMinutes: Int = 5,
    val longBreakMinutes: Int = 15,
    val sessionsUntilLongBreak: Int = 4,
)

enum class PomodoroPhase {
    WORK, SHORT_BREAK, LONG_BREAK;

    fun label(): String = when (this) {
        WORK -> "Focus"
        SHORT_BREAK -> "Short break"
        LONG_BREAK -> "Long break"
    }
}

data class PomodoroState(
    val phase: PomodoroPhase = PomodoroPhase.WORK,
    val secondsLeft: Int = 25 * 60,
    val running: Boolean = false,
    val currentSession: Int = 1,
    val completedSessions: Int = 0,
    val itemTitle: String = "",
    val active: Boolean = false,
    val settings: PomodoroSettings = PomodoroSettings(),
)

// ─── Task input parsing ───────────────────────────────────────────────────────

data class TaskDefaults(
    val morningHour: Int = 8,
    val afternoonHour: Int = 13,
    val nightHour: Int = 21,
    val defaultHour: Int = 10,
)

data class ParsedTask(
    val summary: String,
    val priority: Int,                  // 0, 1(high), 5(med), 9(low)
    val categories: List<String>,
    val due: Instant?,
)

// ─── App preferences ─────────────────────────────────────────────────────────

enum class ThemePreference { SYSTEM, LIGHT, DARK }

data class AppSettings(
    val theme: ThemePreference = ThemePreference.SYSTEM,
    val visibleScreens: List<String> = ALL_SCREENS,
    val kanbanColumns: List<String> = DEFAULT_KANBAN_COLUMNS,
    val pomodoroSettings: PomodoroSettings = PomodoroSettings(),
    val taskDefaults: TaskDefaults = TaskDefaults(),
    val notificationsEnabled: Boolean = true,
    val notificationMinutesBefore: Int = 15,
    val widgetSyncIntervalMinutes: Int = 60,
    val biometricLockEnabled: Boolean = false,
)

// ─── Memos (usememos.com) ─────────────────────────────────────────────────────

@Serializable enum class MemoState { NORMAL, ARCHIVED }
@Serializable enum class MemoVisibility { PRIVATE, PROTECTED, PUBLIC }

@Serializable
data class MemoProperty(
    val hasLink: Boolean = false,
    val hasTaskList: Boolean = false,
    val hasIncompleteTasks: Boolean = false,
    val title: String = "",
)

@Serializable
data class MemosAttachment(
    val name: String,           // "attachments/{id}"
    val filename: String,
    val externalLink: String,
    val type: String,           // MIME type
    val size: Long,
    val memo: String = "",      // "memos/{id}" back-reference
)

data class MemosMemo(
    val name: String,           // "memos/{id}"
    val state: MemoState = MemoState.NORMAL,
    val content: String,
    val visibility: MemoVisibility = MemoVisibility.PRIVATE,
    val tags: List<String> = emptyList(),
    val pinned: Boolean = false,
    val attachments: List<MemosAttachment> = emptyList(),
    val property: MemoProperty = MemoProperty(),
    val snippet: String = "",
    val createTime: Instant,
    val displayTime: Instant,
    val updateTime: Instant,
)

val ALL_SCREENS = listOf("Dashboard", "Inbox", "Events", "Tasks", "Notes", "Issues", "Views", "Memos")
val DEFAULT_KANBAN_COLUMNS = listOf("backlog", "planned", "inprogress", "done")
