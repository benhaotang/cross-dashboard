package com.crossdashboard.app.ui.screen.inbox

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.LazyRow
import androidx.compose.foundation.lazy.items
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.outlined.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.semantics.stateDescription
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.hilt.navigation.compose.hiltViewModel
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.crossdashboard.app.domain.model.*
import com.crossdashboard.app.ui.component.TagFlow
import com.crossdashboard.app.ui.navigation.Destination
import java.time.ZoneId
import java.time.format.DateTimeFormatter

private val DATE_FMT: DateTimeFormatter = DateTimeFormatter.ofPattern("MMM d")
private val TIME_FMT: DateTimeFormatter = DateTimeFormatter.ofPattern("HH:mm")

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun InboxScreen(
    onNavigate: (Destination) -> Unit = {},
    vm: InboxViewModel = hiltViewModel(),
) {
    val state by vm.state.collectAsStateWithLifecycle()
    val zone = ZoneId.systemDefault()
    val totalFormatted = if (state.totalMinutes > 0)
        InboxViewModel.formatMinutes(state.totalMinutes) else null

    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text("Inbox") },
                actions = {
                    totalFormatted?.let { total ->
                        Text(
                            text = total,
                            style = MaterialTheme.typography.labelLarge,
                            color = MaterialTheme.colorScheme.primary,
                            modifier = Modifier
                                .padding(end = 8.dp)
                                .semantics { contentDescription = "Total estimated time: $total" },
                        )
                    }
                },
            )
        },
    ) { padding ->
        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(padding),
        ) {
            // ── Filter chips ──────────────────────────────────────────────────
            FilterRow(filter = state.filter, onFilterChange = vm::setFilter)

            if (state.items.isEmpty()) {
                Box(
                    modifier = Modifier.fillMaxSize(),
                    contentAlignment = Alignment.Center,
                ) {
                    Text(
                        text = "Nothing in inbox",
                        style = MaterialTheme.typography.bodyMedium,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                }
                return@Column
            }

            // ── Item list ─────────────────────────────────────────────────────
            LazyColumn(
                modifier = Modifier.fillMaxSize(),
                contentPadding = PaddingValues(bottom = 80.dp),
            ) {
                items(state.items, key = { itemKey(it) }) { item ->
                    when (item) {
                        is InboxItem.Event -> EventInboxRow(
                            item = item,
                            zone = zone,
                            onClick = { onNavigate(Destination.EventDetail(item.event.uid)) },
                        )
                        is InboxItem.Task -> TaskInboxRow(
                            item = item,
                            zone = zone,
                            magicTags = state.magicTags,
                            onClick = { onNavigate(Destination.TaskDetail(item.task.uid)) },
                        )
                        is InboxItem.Issue -> IssueInboxRow(
                            item = item,
                            magicTags = state.magicTags,
                            onClick = {
                                onNavigate(Destination.IssueDetail(item.issue.id, item.issue.repository))
                            },
                        )
                        is InboxItem.Milestone -> MilestoneInboxRow(item, zone)
                    }
                    HorizontalDivider(thickness = 0.5.dp, color = MaterialTheme.colorScheme.outlineVariant)
                }

                // Footer time total
                totalFormatted?.let { total ->
                    item {
                        Row(
                            modifier = Modifier
                                .fillMaxWidth()
                                .padding(horizontal = 16.dp, vertical = 12.dp)
                                .semantics(mergeDescendants = true) {
                                    contentDescription = "Total estimated time for all items: $total"
                                },
                            horizontalArrangement = Arrangement.End,
                            verticalAlignment = Alignment.CenterVertically,
                        ) {
                            Icon(
                                Icons.Outlined.Timer,
                                contentDescription = null,
                                modifier = Modifier.size(16.dp),
                                tint = MaterialTheme.colorScheme.onSurfaceVariant,
                            )
                            Spacer(Modifier.width(4.dp))
                            Text(
                                text = "Total: $total",
                                style = MaterialTheme.typography.labelMedium,
                                color = MaterialTheme.colorScheme.onSurfaceVariant,
                            )
                        }
                    }
                }
            }
        }
    }
}

// ─── Filter chips ──────────────────────────────────────────────────────────────

@Composable
private fun FilterRow(filter: InboxFilter, onFilterChange: (InboxFilter) -> Unit) {
    LazyRow(
        contentPadding = PaddingValues(horizontal = 12.dp, vertical = 6.dp),
        horizontalArrangement = Arrangement.spacedBy(8.dp),
    ) {
        items(InboxFilter.entries) { f ->
            val label = f.displayName
            FilterChip(
                selected = filter == f,
                onClick = { onFilterChange(f) },
                label = { Text(label) },
                modifier = Modifier.semantics {
                    contentDescription = "Show $label items"
                    stateDescription = if (filter == f) "selected" else "not selected"
                },
            )
        }
    }
}

// ─── Item rows ────────────────────────────────────────────────────────────────

@Composable
private fun EventInboxRow(item: InboxItem.Event, zone: ZoneId, onClick: () -> Unit) {
    val event = item.event
    val startZdt = event.start.atZone(zone)
    val endZdt = event.end.atZone(zone)
    val sameDay = startZdt.toLocalDate() == endZdt.toLocalDate()
    val dateStr = if (sameDay) {
        "${startZdt.format(DATE_FMT)}  ${startZdt.format(TIME_FMT)} – ${endZdt.format(TIME_FMT)}"
    } else {
        "${startZdt.format(DATE_FMT)} – ${endZdt.format(DATE_FMT)}"
    }
    val timeStr = if (item.durationMinutes > 0)
        ", duration ${InboxViewModel.formatMinutes(item.durationMinutes)}" else ""
    val locationStr = event.location?.let { ", at $it" } ?: ""
    val rowDesc = "Event: ${event.summary}, $dateStr$locationStr$timeStr"

    Surface(
        onClick = onClick,
        color = MaterialTheme.colorScheme.surface,
        modifier = Modifier.semantics(mergeDescendants = true) { contentDescription = rowDesc },
    ) {
        ListItem(
        leadingContent = {
            Icon(
                Icons.Outlined.CalendarMonth,
                contentDescription = null,
                tint = MaterialTheme.colorScheme.primary,
            )
        },
        headlineContent = { Text(event.summary, fontWeight = FontWeight.Medium) },
        supportingContent = {
            Column {
                Text(dateStr, style = MaterialTheme.typography.bodySmall)
                event.location?.let { Text(it, style = MaterialTheme.typography.bodySmall) }
            }
        },
        trailingContent = {
            if (item.durationMinutes > 0) {
                Text(
                    InboxViewModel.formatMinutes(item.durationMinutes),
                    style = MaterialTheme.typography.labelSmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
        },
        )
    }
}

@Composable
private fun TaskInboxRow(
    item: InboxItem.Task,
    zone: ZoneId,
    magicTags: List<String>,
    onClick: () -> Unit,
) {
    val task = item.task
    val now = java.time.Instant.now()
    val isOverdue = task.due?.isBefore(now) == true && task.status != TaskStatus.COMPLETED
    val dueStr = task.due?.atZone(zone)?.let { "${it.format(DATE_FMT)} ${it.format(TIME_FMT)}" }
    val overdueStr = if (isOverdue) "overdue, " else ""
    val dueDesc = if (dueStr != null) ", due $dueStr" else ""
    val tagsDesc = if (task.categories.isNotEmpty())
        ", tags: ${task.categories.joinToString { "#${it.trimStart('#')}" }}" else ""
    val timeStr = item.estimatedMinutes?.let { ", ${InboxViewModel.formatMinutes(it)}" } ?: ""
    val rowDesc = "Task: $overdueStr${task.summary}$dueDesc$tagsDesc$timeStr"

    Surface(
        onClick = onClick,
        color = MaterialTheme.colorScheme.surface,
        modifier = Modifier.semantics(mergeDescendants = true) { contentDescription = rowDesc },
    ) {
        ListItem(
        leadingContent = {
            Icon(
                Icons.Outlined.CheckBoxOutlineBlank,
                contentDescription = null,
                tint = if (isOverdue) MaterialTheme.colorScheme.error
                else MaterialTheme.colorScheme.onSurfaceVariant,
            )
        },
        headlineContent = {
            Text(
                task.summary,
                fontWeight = FontWeight.Medium,
                color = if (isOverdue) MaterialTheme.colorScheme.error
                else MaterialTheme.colorScheme.onSurface,
            )
        },
        supportingContent = {
            Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
                if (dueStr != null) {
                    Text(
                        if (isOverdue) "⚠ $dueStr" else dueStr,
                        style = MaterialTheme.typography.bodySmall,
                        color = if (isOverdue) MaterialTheme.colorScheme.error
                        else MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                }
                if (task.categories.isNotEmpty()) {
                    TagFlow(tags = task.categories, magicTags = magicTags)
                }
            }
        },
        trailingContent = {
            item.estimatedMinutes?.let {
                Text(
                    InboxViewModel.formatMinutes(it),
                    style = MaterialTheme.typography.labelSmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
        },
        )
    }
}

@Composable
private fun IssueInboxRow(
    item: InboxItem.Issue,
    magicTags: List<String>,
    onClick: () -> Unit,
) {
    val issue = item.issue
    val labelsDesc = if (issue.labels.isNotEmpty())
        ", labels: ${issue.labels.joinToString()}" else ""
    val timeStr = item.estimatedMinutes?.let { ", ${InboxViewModel.formatMinutes(it)}" } ?: ""
    val rowDesc = "Issue: ${issue.title}, ${issue.state}, ${issue.repository}$labelsDesc$timeStr"

    Surface(
        onClick = onClick,
        color = MaterialTheme.colorScheme.surface,
        modifier = Modifier.semantics(mergeDescendants = true) { contentDescription = rowDesc },
    ) {
        ListItem(
        leadingContent = {
            Icon(
                Icons.Outlined.BugReport,
                contentDescription = null,
                tint = MaterialTheme.colorScheme.tertiary,
            )
        },
        headlineContent = { Text(issue.title, fontWeight = FontWeight.Medium) },
        supportingContent = {
            Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
                Text(
                    issue.repository,
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
                if (issue.labels.isNotEmpty()) {
                    TagFlow(tags = issue.labels, magicTags = magicTags)
                }
            }
        },
        trailingContent = {
            item.estimatedMinutes?.let {
                Text(
                    InboxViewModel.formatMinutes(it),
                    style = MaterialTheme.typography.labelSmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
        },
        )
    }
}

@Composable
private fun MilestoneInboxRow(item: InboxItem.Milestone, zone: ZoneId) {
    val ms = item.milestone
    val dueStr = ms.dueOn?.atZone(zone)?.format(DATE_FMT)
    val dueDesc = if (dueStr != null) ", due $dueStr" else ""
    val rowDesc = "Milestone: ${ms.title}, ${ms.openIssues} open, ${ms.closedIssues} closed$dueDesc"

    ListItem(
        modifier = Modifier.semantics(mergeDescendants = true) { contentDescription = rowDesc },
        leadingContent = {
            Icon(
                Icons.Outlined.Flag,
                contentDescription = null,
                tint = MaterialTheme.colorScheme.secondary,
            )
        },
        headlineContent = { Text(ms.title, fontWeight = FontWeight.Medium) },
        supportingContent = {
            Text(
                "${ms.openIssues} open · ${ms.closedIssues} closed" +
                    (if (dueStr != null) " · due $dueStr" else ""),
                style = MaterialTheme.typography.bodySmall,
            )
        },
    )
}

// ─── Key helpers ──────────────────────────────────────────────────────────────

private fun itemKey(item: InboxItem): String = when (item) {
    is InboxItem.Event -> "event_${item.event.uid}"
    is InboxItem.Task -> "task_${item.task.uid}"
    is InboxItem.Issue -> "issue_${item.issue.id}"
    is InboxItem.Milestone -> "milestone_${item.milestone.id}"
}
