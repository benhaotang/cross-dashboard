package com.crossdashboard.app.ui.screen.inbox

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.crossdashboard.app.data.prefs.AppPreferences
import com.crossdashboard.app.data.repository.EventRepository
import com.crossdashboard.app.data.repository.IssueRepository
import com.crossdashboard.app.data.repository.TaskRepository
import com.crossdashboard.app.domain.model.*
import dagger.hilt.android.lifecycle.HiltViewModel
import kotlinx.coroutines.flow.*
import kotlinx.coroutines.launch
import java.time.Duration
import java.time.Instant
import java.time.DayOfWeek
import java.time.ZoneId
import java.time.temporal.TemporalAdjusters
import javax.inject.Inject

enum class InboxFilter(val displayName: String) {
    ALL("All"),
    EVENTS("Events"),
    TASKS("Tasks"),
    TASKS_TODAY("Today"),
    TASKS_TOMORROW("Tomorrow"),
    TASKS_THIS_WEEK("This Week"),
    ISSUES("Issues"),
}

data class InboxUiState(
    val items: List<InboxItem> = emptyList(),
    val filter: InboxFilter = InboxFilter.ALL,
    /** Sum of all timed items in minutes (0 = nothing timed) */
    val totalMinutes: Int = 0,
    val magicTags: List<String> = emptyList(),
    val isLoading: Boolean = false,
    val error: String? = null,
)

@HiltViewModel
class InboxViewModel @Inject constructor(
    private val eventRepo: EventRepository,
    private val taskRepo: TaskRepository,
    private val issueRepo: IssueRepository,
    private val prefs: AppPreferences,
) : ViewModel() {

    private val _filter = MutableStateFlow(InboxFilter.ALL)
    private val _state = MutableStateFlow(InboxUiState())
    val state: StateFlow<InboxUiState> = _state.asStateFlow()

    init {
        viewModelScope.launch {
            combine(
                eventRepo.events,
                taskRepo.allTasks,
                issueRepo.allIssues,
                _filter,
                prefs.kanbanColumnsFlow,
            ) { events, tasks, issues, filter, kanbanColumns ->
                buildState(events, tasks, issues, filter, kanbanColumns)
            }.collect { newState ->
                _state.value = newState
            }
        }
    }

    fun setFilter(filter: InboxFilter) {
        _filter.value = filter
    }

    fun dismissError() = _state.update { it.copy(error = null) }

    // ─── Private helpers ──────────────────────────────────────────────────────

    private fun buildState(
        events: List<CalendarEvent>,
        tasks: List<CalDavTask>,
        issues: List<GiteaIssue>,
        filter: InboxFilter,
        kanbanColumns: List<String>,
    ): InboxUiState {
        val now = Instant.now()
        val cutoff = now.minus(Duration.ofDays(1))
        val zone = ZoneId.systemDefault()
        val today = now.atZone(zone).toLocalDate()
        val todayStart = today.atStartOfDay(zone).toInstant()
        val tomorrowStart = today.plusDays(1).atStartOfDay(zone).toInstant()
        val dayAfterTomorrowStart = today.plusDays(2).atStartOfDay(zone).toInstant()
        val weekStart = today.with(TemporalAdjusters.previousOrSame(DayOfWeek.MONDAY))
            .atStartOfDay(zone).toInstant()
        val nextWeekStart = today.with(TemporalAdjusters.next(DayOfWeek.MONDAY))
            .atStartOfDay(zone).toInstant()

        val eventItems: List<InboxItem> = events
            .filter { it.end.isAfter(cutoff) }
            .map { event ->
                val dur = Duration.between(event.start, event.end)
                // Exclude all-day events (≥24h) from time totals by recording 0
                val minutes = if (dur.toHours() >= 24) 0 else dur.toMinutes().toInt().coerceAtLeast(0)
                InboxItem.Event(event, minutes)
            }
            .sortedBy { it.event.start }

        val taskItems: List<InboxItem.Task> = tasks
            .filter { it.status != TaskStatus.COMPLETED && it.status != TaskStatus.CANCELLED }
            .map { task -> InboxItem.Task(task, parseTimeEstimate(task.categories)) }
            .sortedWith(compareBy(nullsLast()) { it.task.due })

        val issueItems: List<InboxItem> = issues
            .filter { it.state == "open" }
            .map { issue -> InboxItem.Issue(issue, parseTimeEstimate(issue.labels)) }
            .sortedByDescending { (it as InboxItem.Issue).issue.updatedAt }

        val filtered: List<InboxItem> = when (filter) {
            InboxFilter.ALL -> eventItems + taskItems + issueItems
            InboxFilter.EVENTS -> eventItems
            InboxFilter.TASKS -> taskItems
            InboxFilter.TASKS_TODAY -> taskItems.filter {
                it.task.due?.let { due -> due >= todayStart && due < tomorrowStart } == true
            }
            InboxFilter.TASKS_TOMORROW -> taskItems.filter {
                it.task.due?.let { due -> due >= tomorrowStart && due < dayAfterTomorrowStart } == true
            }
            InboxFilter.TASKS_THIS_WEEK -> taskItems.filter {
                it.task.due?.let { due -> due >= weekStart && due < nextWeekStart } == true
            }
            InboxFilter.ISSUES -> issueItems
        }

        val total = filtered.sumOf { item ->
            when (item) {
                is InboxItem.Event -> if (item.durationMinutes > 0) item.durationMinutes else 0
                is InboxItem.Task -> item.estimatedMinutes ?: 0
                is InboxItem.Issue -> item.estimatedMinutes ?: 0
                is InboxItem.Milestone -> 0
            }
        }

        return InboxUiState(
            items = filtered,
            filter = filter,
            totalMinutes = total,
            magicTags = kanbanColumns,
        )
    }

    companion object {
        private val TIME_TAG_REGEX = Regex("""#?(\d+)(m|h)$""", RegexOption.IGNORE_CASE)

        /** Parse first `#Xm` or `#Xh` tag in a list. Returns minutes or null. */
        fun parseTimeEstimate(tags: List<String>): Int? {
            for (tag in tags) {
                val m = TIME_TAG_REGEX.find(tag.trimStart('#')) ?: continue
                val amount = m.groupValues[1].toIntOrNull() ?: continue
                return if (m.groupValues[2].equals("h", ignoreCase = true)) amount * 60 else amount
            }
            return null
        }

        /** Format total minutes as "Xh Ym" or "Ym". */
        fun formatMinutes(minutes: Int): String {
            val h = minutes / 60
            val m = minutes % 60
            return when {
                h > 0 && m > 0 -> "${h}h ${m}m"
                h > 0 -> "${h}h"
                else -> "${m}m"
            }
        }
    }
}
