package com.crossdashboard.app.ui.screen.inbox

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.crossdashboard.app.data.repository.EventRepository
import com.crossdashboard.app.data.repository.IssueRepository
import com.crossdashboard.app.data.repository.TaskRepository
import com.crossdashboard.app.domain.model.*
import dagger.hilt.android.lifecycle.HiltViewModel
import kotlinx.coroutines.flow.*
import kotlinx.coroutines.launch
import java.time.Duration
import java.time.Instant
import javax.inject.Inject

enum class InboxFilter { ALL, EVENTS, TASKS, ISSUES }

data class InboxUiState(
    val items: List<InboxItem> = emptyList(),
    val filter: InboxFilter = InboxFilter.ALL,
    /** Sum of all timed items in minutes (0 = nothing timed) */
    val totalMinutes: Int = 0,
    val isLoading: Boolean = false,
    val error: String? = null,
)

@HiltViewModel
class InboxViewModel @Inject constructor(
    private val eventRepo: EventRepository,
    private val taskRepo: TaskRepository,
    private val issueRepo: IssueRepository,
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
            ) { events, tasks, issues, filter ->
                buildState(events, tasks, issues, filter)
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
    ): InboxUiState {
        val now = Instant.now()
        val cutoff = now.minus(Duration.ofDays(1))

        val eventItems: List<InboxItem> = events
            .filter { it.end.isAfter(cutoff) }
            .map { event ->
                val dur = Duration.between(event.start, event.end)
                // Exclude all-day events (≥24h) from time totals by recording 0
                val minutes = if (dur.toHours() >= 24) 0 else dur.toMinutes().toInt().coerceAtLeast(0)
                InboxItem.Event(event, minutes)
            }
            .sortedBy { it.event.start }

        val taskItems: List<InboxItem> = tasks
            .filter { it.status != TaskStatus.COMPLETED && it.status != TaskStatus.CANCELLED }
            .map { task -> InboxItem.Task(task, parseTimeEstimate(task.categories)) }
            .sortedWith(compareBy(nullsLast()) { (it as InboxItem.Task).task.due })

        val issueItems: List<InboxItem> = issues
            .filter { it.state == "open" }
            .map { issue -> InboxItem.Issue(issue, parseTimeEstimate(issue.labels)) }
            .sortedByDescending { (it as InboxItem.Issue).issue.updatedAt }

        val filtered: List<InboxItem> = when (filter) {
            InboxFilter.ALL -> eventItems + taskItems + issueItems
            InboxFilter.EVENTS -> eventItems
            InboxFilter.TASKS -> taskItems
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

        return InboxUiState(items = filtered, filter = filter, totalMinutes = total)
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
