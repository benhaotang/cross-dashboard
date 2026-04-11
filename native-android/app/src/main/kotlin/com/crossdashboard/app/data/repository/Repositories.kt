package com.crossdashboard.app.data.repository

import com.crossdashboard.app.data.db.*
import com.crossdashboard.app.data.db.dao.*
import com.crossdashboard.app.data.network.CalDavClient
import com.crossdashboard.app.data.network.GiteaClient
import com.crossdashboard.app.data.prefs.AppPreferences
import com.crossdashboard.app.data.prefs.CredentialKey
import com.crossdashboard.app.data.prefs.SecureStore
import com.crossdashboard.app.domain.model.*
import kotlinx.coroutines.flow.*
import kotlinx.serialization.json.Json
import java.time.Instant
import java.time.LocalDate
import java.time.ZoneId
import java.time.temporal.ChronoUnit
import javax.inject.Inject
import javax.inject.Singleton

// ─── Event Repository ─────────────────────────────────────────────────────────

@Singleton
class EventRepository @Inject constructor(
    private val dao: EventDao,
    private val client: CalDavClient,
    private val secureStore: SecureStore,
) {
    val events: Flow<List<CalendarEvent>> = dao.observeAll().map { list ->
        list.map { it.toDomain() }
    }

    suspend fun sync(calendarHrefs: List<String>) {
        if (calendarHrefs.isEmpty()) return
        val from = Instant.now().minus(30, ChronoUnit.DAYS)
        val to = Instant.now().plus(180, ChronoUnit.DAYS)
        val fresh = client.fetchEvents(calendarHrefs, from, to)
        if (fresh.isNotEmpty() || dao.getUpcoming(0, 1).isNotEmpty()) {
            dao.deleteAll()
            dao.upsertAll(fresh.map { it.toEntity() })
        }
    }

    suspend fun getUpcoming(limit: Int = 5): List<CalendarEvent> {
        return dao.getUpcoming(Instant.now().toEpochMilli(), limit).map { it.toDomain() }
    }

    suspend fun create(event: CalendarEvent, calendarHref: String): CalendarEvent {
        val saved = client.createEvent(event, calendarHref)
        dao.upsertAll(listOf(saved.toEntity()))
        return saved
    }

    suspend fun delete(event: CalendarEvent) {
        client.deleteEvent(event)
        dao.deleteAll() // will re-sync
    }

    private fun selectedCalendarsFromStore(): List<String> {
        val raw = secureStore.get(CredentialKey.CALDAV_SELECTED_CALENDARS) ?: return emptyList()
        return runCatching {
            Json.decodeFromString<List<CalDavCalendar>>(raw).map { it.href }
        }.getOrDefault(emptyList())
    }
}

// ─── Task Repository ──────────────────────────────────────────────────────────

@Singleton
class TaskRepository @Inject constructor(
    private val dao: TaskDao,
    private val statsDao: DailyStatsDao,
    private val client: CalDavClient,
) {
    val allTasks: Flow<List<CalDavTask>> = dao.observeAll().map { it.map { e -> e.toDomain() } }
    val activeTasks: Flow<List<CalDavTask>> = dao.observeActive().map { it.map { e -> e.toDomain() } }
    val completedTasks: Flow<List<CalDavTask>> = dao.observeCompleted().map { it.map { e -> e.toDomain() } }

    fun subtasksOf(parentUid: String): Flow<List<CalDavTask>> =
        dao.observeSubtasks(parentUid).map { it.map { e -> e.toDomain() } }

    suspend fun sync(calendarHrefs: List<String>) {
        if (calendarHrefs.isEmpty()) return
        val fresh = client.fetchTasks(calendarHrefs)
        if (fresh.isNotEmpty() || dao.getDueSoon(Long.MAX_VALUE, 1).isNotEmpty()) {
            dao.deleteAll()
            dao.upsertAll(fresh.map { it.toEntity() })
        }
    }

    suspend fun create(task: CalDavTask, calendarHref: String): CalDavTask {
        val saved = client.createTask(task, calendarHref)
        dao.upsertAll(listOf(saved.toEntity()))
        return saved
    }

    suspend fun update(task: CalDavTask) {
        client.updateTask(task)
        dao.upsertAll(listOf(task.toEntity()))
    }

    suspend fun delete(task: CalDavTask) {
        client.deleteTask(task)
        dao.deleteByUid(task.uid)
    }

    suspend fun toggleComplete(task: CalDavTask): CalDavTask {
        val wasCompleted = task.status == TaskStatus.COMPLETED
        val updated = if (wasCompleted) {
            task.copy(status = TaskStatus.NEEDS_ACTION, completed = null, percentComplete = 0)
        } else {
            task.copy(status = TaskStatus.COMPLETED, completed = Instant.now(), percentComplete = 100)
        }
        update(updated)
        if (!wasCompleted) {
            val today = LocalDate.now(ZoneId.systemDefault()).toString()
            statsDao.increment(today, StatField.TASKS_COMPLETED)
        }
        return updated
    }

    suspend fun getDueSoon(deadlineEpoch: Long, limit: Int = 5) =
        dao.getDueSoon(deadlineEpoch, limit).map { it.toDomain() }
}

// ─── Note Repository ──────────────────────────────────────────────────────────

@Singleton
class NoteRepository @Inject constructor(
    private val dao: NoteDao,
    private val client: CalDavClient,
) {
    val notes: Flow<List<Note>> = dao.observeAll().map { it.map { e -> e.toDomain() } }

    suspend fun sync(calendarHrefs: List<String>) {
        if (calendarHrefs.isEmpty()) return
        val fresh = client.fetchNotes(calendarHrefs)
        if (fresh.isNotEmpty() || dao.getByUid("_") == null) {
            dao.deleteAll()
            dao.upsertAll(fresh.map { it.toEntity() })
        }
    }

    suspend fun create(note: Note, calendarHref: String): Note {
        val saved = client.createNote(note, calendarHref)
        dao.upsertAll(listOf(saved.toEntity()))
        return saved
    }

    suspend fun update(note: Note) {
        client.updateNote(note)
        dao.upsertAll(listOf(note.toEntity()))
    }

    suspend fun delete(note: Note) {
        client.deleteNote(note)
        dao.deleteByUid(note.uid)
    }
}

// ─── Issue Repository ─────────────────────────────────────────────────────────

@Singleton
class IssueRepository @Inject constructor(
    private val dao: IssueDao,
    private val statsDao: DailyStatsDao,
    private val client: GiteaClient,
) {
    val allIssues: Flow<List<GiteaIssue>> = dao.observeAll().map { it.map { e -> e.toDomain() } }
    val openIssues: Flow<List<GiteaIssue>> = dao.observeOpen().map { it.map { e -> e.toDomain() } }

    suspend fun sync(repositories: List<String>) {
        if (repositories.isEmpty()) return
        val open = client.fetchIssues(repositories, "open")
        val closed = client.fetchIssues(repositories, "closed")
        val all = open + closed
        if (all.isNotEmpty()) {
            dao.deleteAll()
            dao.upsertAll(all.map { it.toEntity() })
        }
    }

    suspend fun update(
        repo: String, number: Int,
        title: String? = null, body: String? = null, state: String? = null,
    ): GiteaIssue {
        val updated = client.updateIssue(repo, number, title, body, state)
        dao.upsertAll(listOf(updated.toEntity()))
        if (state == "closed") {
            val today = LocalDate.now(ZoneId.systemDefault()).toString()
            statsDao.increment(today, StatField.ISSUES_CLOSED)
        }
        return updated
    }

    suspend fun fetchComments(repo: String, number: Int) = client.fetchComments(repo, number)

    suspend fun addComment(repo: String, number: Int, body: String) =
        client.addComment(repo, number, body)

    suspend fun replaceLabels(repo: String, number: Int, labelNames: List<String>) {
        val existingLabels = client.fetchLabels(repo)
        val labelIds = labelNames.map { name ->
            existingLabels.firstOrNull { it.name == name }?.id
                ?: client.createRepoLabel(repo, name).id
        }
        client.replaceIssueLabels(repo, number, labelIds)
        // Reload this issue from server
        val fresh = client.fetchIssues(listOf(repo), "open") + client.fetchIssues(listOf(repo), "closed")
        val updated = fresh.firstOrNull { it.number == number }
        if (updated != null) dao.upsertAll(listOf(updated.toEntity()))
    }
}

// ─── Stats Repository ─────────────────────────────────────────────────────────

@Singleton
class StatsRepository @Inject constructor(
    private val dao: DailyStatsDao,
) {
    suspend fun getRange(startDaysAgo: Int): List<DailyStats> {
        val start = LocalDate.now(ZoneId.systemDefault()).minusDays(startDaysAgo.toLong())
        return dao.getRange(start.toString()).map {
            DailyStats(
                date = LocalDate.parse(it.date),
                tasksCompleted = it.tasksCompleted,
                pomodoroSessions = it.pomodoroSessions,
                issuesClosed = it.issuesClosed,
            )
        }
    }

    suspend fun incrementPomodoro() {
        val today = LocalDate.now(ZoneId.systemDefault()).toString()
        dao.increment(today, StatField.POMODORO_SESSIONS)
    }
}
