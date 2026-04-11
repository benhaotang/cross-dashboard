package com.crossdashboard.app.ui.screen.dashboard

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.crossdashboard.app.data.prefs.AppPreferences
import com.crossdashboard.app.data.repository.EventRepository
import com.crossdashboard.app.data.repository.IssueRepository
import com.crossdashboard.app.data.repository.StatsRepository
import com.crossdashboard.app.data.repository.TaskRepository
import com.crossdashboard.app.domain.model.CalendarEvent
import com.crossdashboard.app.domain.model.CalDavTask
import com.crossdashboard.app.domain.model.DailyStats
import dagger.hilt.android.lifecycle.HiltViewModel
import kotlinx.coroutines.flow.*
import kotlinx.coroutines.launch
import java.time.Instant
import java.time.LocalDate
import java.time.ZoneId
import java.time.temporal.ChronoUnit
import javax.inject.Inject

data class StatsCard(
    val tasksCompleted: Int,
    val pomodoroSessions: Int,
    val issuesClosed: Int,
    val tasksDelta: Int,      // vs previous 7 days (positive = up)
    val pomodorosDelta: Int,
    val issuesDelta: Int,
    val hasPriorData: Boolean,
)

data class DashboardUiState(
    val upcomingEvents: List<CalendarEvent> = emptyList(),
    val dueTasks: List<CalDavTask> = emptyList(),
    val openIssuesCount: Int = 0,
    val statsCard: StatsCard? = null,
    val lastSync: Long? = null,
    val isCalDavConfigured: Boolean = false,
    val isGiteaConfigured: Boolean = false,
)

@HiltViewModel
class DashboardViewModel @Inject constructor(
    private val eventRepo: EventRepository,
    private val taskRepo: TaskRepository,
    private val issueRepo: IssueRepository,
    private val statsRepo: StatsRepository,
    private val prefs: AppPreferences,
) : ViewModel() {

    private val _state = MutableStateFlow(DashboardUiState())
    val state: StateFlow<DashboardUiState> = _state.asStateFlow()

    init {
        viewModelScope.launch {
            // Combine the live Room flows
            combine(
                eventRepo.events,
                taskRepo.activeTasks,
                issueRepo.openIssues,
                prefs.lastSyncFlow,
            ) { events, tasks, issues, lastSync ->
                val now = Instant.now()
                val in7Days = now.plus(7, ChronoUnit.DAYS)

                val upcoming = events
                    .filter { it.start.isAfter(now) && it.start.isBefore(in7Days) }
                    .sortedBy { it.start }
                    .take(5)

                val dueSoon = tasks
                    .filter { it.due != null && it.due.isBefore(in7Days) }
                    .sortedBy { it.due }
                    .take(5)

                Triple(upcoming, dueSoon, Pair(issues.size, lastSync))
            }.collect { (upcoming, dueSoon, issuesPair) ->
                val (issuesCount, lastSync) = issuesPair
                _state.update {
                    it.copy(
                        upcomingEvents = upcoming,
                        dueTasks = dueSoon,
                        openIssuesCount = issuesCount,
                        lastSync = lastSync,
                    )
                }
                refreshStats()
            }
        }
    }

    private suspend fun refreshStats() {
        // Get last 14 days of data for this7 vs prev7 comparison
        // Fetch last 14 days (0..13 days ago) to compare this7 vs prev7
        val allStats = statsRepo.getRange(startDaysAgo = 13)
        val today = LocalDate.now(ZoneId.systemDefault())

        // sumRange: newest startDaysAgo=0 means today; count=7 covers today back 6 days
        fun sumRange(newestDaysAgo: Int, oldestDaysAgo: Int): Triple<Int, Int, Int> {
            val newest = today.minusDays(newestDaysAgo.toLong())
            val oldest = today.minusDays(oldestDaysAgo.toLong())
            val inRange = allStats.filter { it.date in oldest..newest }
            return Triple(
                inRange.sumOf { it.tasksCompleted },
                inRange.sumOf { it.pomodoroSessions },
                inRange.sumOf { it.issuesClosed },
            )
        }

        val (t7tasks, t7pomos, t7issues) = sumRange(newestDaysAgo = 0, oldestDaysAgo = 6)
        val (p7tasks, p7pomos, p7issues) = sumRange(newestDaysAgo = 7, oldestDaysAgo = 13)
        val hasPrior = p7tasks + p7pomos + p7issues > 0

        _state.update {
            it.copy(
                statsCard = StatsCard(
                    tasksCompleted = t7tasks,
                    pomodoroSessions = t7pomos,
                    issuesClosed = t7issues,
                    tasksDelta = t7tasks - p7tasks,
                    pomodorosDelta = t7pomos - p7pomos,
                    issuesDelta = t7issues - p7issues,
                    hasPriorData = hasPrior,
                ),
            )
        }
    }
}
