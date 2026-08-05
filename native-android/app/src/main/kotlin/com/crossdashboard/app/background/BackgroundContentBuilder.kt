package com.crossdashboard.app.background

import com.crossdashboard.app.data.repository.EventRepository
import com.crossdashboard.app.data.repository.IssueRepository
import com.crossdashboard.app.data.repository.TaskRepository
import com.crossdashboard.app.data.prefs.AppPreferences
import com.crossdashboard.app.domain.model.TaskStatus
import com.crossdashboard.app.ui.screen.inbox.InboxViewModel
import kotlinx.coroutines.flow.first
import java.time.*
import java.time.format.DateTimeFormatter
import java.time.temporal.TemporalAdjusters
import javax.inject.Inject
import javax.inject.Singleton

@Singleton
class BackgroundContentBuilder @Inject constructor(
    private val events: EventRepository,
    private val tasks: TaskRepository,
    private val issues: IssueRepository,
    private val prefs: AppPreferences,
) {
    suspend fun build(template: BackgroundTemplate): BackgroundContent = when (template.source) {
        BackgroundSource.INBOX -> buildInbox(template)
        BackgroundSource.VIEWS -> buildViews(template)
    }

    private suspend fun buildInbox(t: BackgroundTemplate): BackgroundContent {
        val now = Instant.now()
        data class TimedRow(val row: BackgroundRow, val date: Instant?, val minutes: Int)
        val rows = mutableListOf<TimedRow>()
        val eventValues = events.events.first()
        val taskValues = tasks.allTasks.first()
        val issueValues = issues.allIssues.first()
        if (t.inboxTypeFilter == "ALL" || t.inboxTypeFilter == "EVENTS") {
            eventValues.filter { it.end.isAfter(now.minus(Duration.ofDays(1))) }.sortedBy { it.start }.forEach { event ->
                val duration = Duration.between(event.start, event.end).toMinutes().toInt().takeIf { it in 1..<1440 } ?: 0
                rows += TimedRow(BackgroundRow(event.summary, formatDate(event.start), BackgroundRow.Kind.EVENT), event.start, duration)
            }
        }
        if (t.inboxTypeFilter == "ALL" || t.inboxTypeFilter == "TASKS") {
            taskValues.filter { it.status != TaskStatus.COMPLETED && it.status != TaskStatus.CANCELLED }
                .sortedBy { it.due }.forEach { task ->
                    rows += TimedRow(BackgroundRow(task.summary, task.due?.let(::formatDate) ?: "No due date", BackgroundRow.Kind.TASK,
                        overdue = task.due?.isBefore(now) == true), task.due, InboxViewModel.parseTimeEstimate(task.categories) ?: 0)
                }
        }
        if (t.inboxTypeFilter == "ALL" || t.inboxTypeFilter == "ISSUES") {
            issueValues.filter { it.state == "open" }.sortedByDescending { it.updatedAt }.forEach { issue ->
                rows += TimedRow(BackgroundRow(issue.title, issue.repository, BackgroundRow.Kind.ISSUE), issue.milestoneDueOn,
                    InboxViewModel.parseTimeEstimate(issue.labels) ?: 0)
            }
        }
        val filtered = rows.filter { matchesDate(it.date, t.inboxDateFilter) }
        return BackgroundContent("INBOX", "${label(t.inboxTypeFilter)} · ${label(t.inboxDateFilter)}",
            rows = filtered.map { it.row }, totalMinutes = filtered.sumOf { it.minutes })
    }

    private suspend fun buildViews(t: BackgroundTemplate): BackgroundContent {
        val columns = prefs.kanbanColumnsFlow.first()
        val rows = mutableListOf<BackgroundRow>()
        if (t.viewsTypeFilter != "ISSUES") {
            tasks.allTasks.first().filter { it.status != TaskStatus.COMPLETED && it.status != TaskStatus.CANCELLED && matchesDate(it.due, t.viewsDateFilter) }
                .forEach { task ->
                    val group = if (t.viewsMode == "COVEY") {
                        when {
                            "do" in task.categories -> "Do First"
                            "delay" in task.categories -> "Schedule"
                            "delegate" in task.categories -> "Delegate"
                            "eliminate" in task.categories -> "Eliminate"
                            else -> "Untagged"
                        }
                    } else task.categories.firstOrNull { it in columns } ?: "Untagged"
                    if (t.viewsMode != "COVEY" || group != "Untagged") {
                        rows += BackgroundRow(task.summary, task.due?.let(::formatDate) ?: "Task", BackgroundRow.Kind.TASK, group)
                    }
                }
        }
        if (t.viewsTypeFilter != "TASKS") {
            issues.allIssues.first().filter { it.state == "open" && matchesDate(it.milestoneDueOn, t.viewsDateFilter) }.forEach { issue ->
                val group = if (t.viewsMode == "COVEY") {
                    when {
                        "do" in issue.labels -> "Do First"
                        "delay" in issue.labels -> "Schedule"
                        "delegate" in issue.labels -> "Delegate"
                        "eliminate" in issue.labels -> "Eliminate"
                        else -> "Untagged"
                    }
                } else issue.labels.firstOrNull { it in columns } ?: "Untagged"
                if (t.viewsMode != "COVEY" || group != "Untagged") {
                    rows += BackgroundRow(issue.title, issue.repository, BackgroundRow.Kind.ISSUE, group)
                }
            }
        }
        return BackgroundContent("VIEWS", "${label(t.viewsTypeFilter)} · ${label(t.viewsDateFilter)}", label(t.viewsMode), rows)
    }

    private fun matchesDate(value: Instant?, filter: String): Boolean {
        if (filter == "ALL") return true
        if (value == null) return false
        val zone = ZoneId.systemDefault()
        val today = LocalDate.now(zone)
        val start = today.atStartOfDay(zone).toInstant()
        val tomorrow = today.plusDays(1).atStartOfDay(zone).toInstant()
        return when (filter) {
            "TODAY" -> value >= start && value < tomorrow
            "TOMORROW" -> value >= tomorrow && value < today.plusDays(2).atStartOfDay(zone).toInstant()
            else -> {
                val week = today.with(TemporalAdjusters.previousOrSame(DayOfWeek.MONDAY)).atStartOfDay(zone).toInstant()
                value >= week && value < week.plus(Duration.ofDays(7))
            }
        }
    }

    private fun formatDate(value: Instant): String = DATE_FORMAT.format(value.atZone(ZoneId.systemDefault()))
    private fun label(value: String) = value.lowercase().replace('_', ' ').replaceFirstChar { it.uppercase() }

    companion object { private val DATE_FORMAT = DateTimeFormatter.ofPattern("MMM d · HH:mm") }
}
