package com.crossdashboard.app.ui.screen.dashboard

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.outlined.Refresh
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.hilt.navigation.compose.hiltViewModel
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.crossdashboard.app.domain.model.CalDavTask
import com.crossdashboard.app.domain.model.CalendarEvent
import com.crossdashboard.app.ui.navigation.Destination
import java.time.ZoneId
import java.time.format.DateTimeFormatter

@Composable
fun DashboardScreen(
    onNavigate: (Destination) -> Unit = {},
    viewModel: DashboardViewModel = hiltViewModel(),
) {
    val state by viewModel.state.collectAsStateWithLifecycle()

    Scaffold(
        topBar = {
            DashboardTopBar(onRefresh = viewModel::syncNow)
        },
    ) { paddingValues ->
        LazyColumn(
            modifier = Modifier
                .fillMaxSize()
                .padding(paddingValues)
                .padding(horizontal = 16.dp),
            verticalArrangement = Arrangement.spacedBy(16.dp),
            contentPadding = PaddingValues(vertical = 16.dp),
        ) {
            // ── Stats card ───────────────────────────────────────────────────
            state.statsCard?.let { card ->
                item {
                    StatsCard(card = card)
                }
            }

            // ── Upcoming events ──────────────────────────────────────────────
            item {
                SectionTitle(title = "Upcoming Events")
            }
            if (state.upcomingEvents.isEmpty()) {
                item {
                    EmptyHint("No events in the next 7 days")
                }
            } else {
                items(state.upcomingEvents, key = { it.uid }) { event ->
                    EventRow(event = event, onClick = { onNavigate(Destination.EventDetail(event.uid)) })
                }
            }

            // ── Tasks due soon ───────────────────────────────────────────────
            item {
                SectionTitle(title = "Tasks Due Soon")
            }
            if (state.dueTasks.isEmpty()) {
                item {
                    EmptyHint("No tasks due this week")
                }
            } else {
                items(state.dueTasks, key = { it.uid }) { task ->
                    TaskRow(task = task, onClick = { onNavigate(Destination.TaskDetail(task.uid)) })
                }
            }

            // ── Open issues count ────────────────────────────────────────────
            if (state.openIssuesCount > 0) {
                item {
                    SectionTitle(title = "Issues")
                    Surface(
                        shape = MaterialTheme.shapes.medium,
                        color = MaterialTheme.colorScheme.errorContainer,
                        modifier = Modifier.fillMaxWidth(),
                    ) {
                        Row(
                            modifier = Modifier.padding(16.dp),
                            verticalAlignment = Alignment.CenterVertically,
                        ) {
                            Text(
                                text = "${state.openIssuesCount}",
                                style = MaterialTheme.typography.headlineMedium,
                                color = MaterialTheme.colorScheme.onErrorContainer,
                                fontWeight = FontWeight.Bold,
                            )
                            Spacer(Modifier.width(8.dp))
                            Text(
                                text = "open issue${if (state.openIssuesCount != 1) "s" else ""}",
                                style = MaterialTheme.typography.bodyMedium,
                                color = MaterialTheme.colorScheme.onErrorContainer,
                            )
                        }
                    }
                }
            }

            item { Spacer(Modifier.height(80.dp)) }
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun DashboardTopBar(onRefresh: () -> Unit) {
    TopAppBar(
        title = { Text("Dashboard") },
        actions = {
            IconButton(
                onClick = onRefresh,
                modifier = Modifier.semantics { contentDescription = "Sync all data" },
            ) {
                Icon(Icons.Outlined.Refresh, contentDescription = null)
            }
        },
    )
}

@Composable
private fun SectionTitle(title: String) {
    Text(
        text = title,
        style = MaterialTheme.typography.titleSmall,
        color = MaterialTheme.colorScheme.primary,
        fontWeight = FontWeight.SemiBold,
        modifier = Modifier.padding(bottom = 8.dp),
    )
}

@Composable
private fun EmptyHint(text: String) {
    Text(
        text = text,
        style = MaterialTheme.typography.bodyMedium,
        color = MaterialTheme.colorScheme.onSurfaceVariant,
        modifier = Modifier.padding(start = 4.dp, bottom = 4.dp),
    )
}

// ─── Stats card ───────────────────────────────────────────────────────────────

@Composable
private fun StatsCard(card: StatsCard) {
    Surface(
        shape = MaterialTheme.shapes.large,
        tonalElevation = 2.dp,
        modifier = Modifier.fillMaxWidth(),
    ) {
        Column(modifier = Modifier.padding(16.dp)) {
            Text(
                text = "Last 7 Days",
                style = MaterialTheme.typography.titleSmall,
                fontWeight = FontWeight.SemiBold,
            )
            Spacer(Modifier.height(12.dp))
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceEvenly,
            ) {
                StatTile(
                    label = "Tasks done",
                    value = card.tasksCompleted,
                    delta = card.tasksDelta,
                    hasPrior = card.hasPriorData,
                )
                StatTile(
                    label = "Pomodoros",
                    value = card.pomodoroSessions,
                    delta = card.pomodorosDelta,
                    hasPrior = card.hasPriorData,
                )
                StatTile(
                    label = "Issues closed",
                    value = card.issuesClosed,
                    delta = card.issuesDelta,
                    hasPrior = card.hasPriorData,
                )
            }
        }
    }
}

@Composable
private fun StatTile(
    label: String,
    value: Int,
    delta: Int,
    hasPrior: Boolean,
) {
    val deltaColor = when {
        !hasPrior -> MaterialTheme.colorScheme.onSurfaceVariant
        delta > 0 -> MaterialTheme.colorScheme.tertiary
        delta < 0 -> MaterialTheme.colorScheme.error
        else -> MaterialTheme.colorScheme.onSurfaceVariant
    }
    val deltaArrow = when {
        !hasPrior || delta == 0 -> ""
        delta > 0 -> "↑${delta}"
        else -> "↓${-delta}"
    }

    Column(
        horizontalAlignment = Alignment.CenterHorizontally,
        modifier = Modifier.semantics(mergeDescendants = true) {
            contentDescription = buildString {
                append("$label: $value")
                if (deltaArrow.isNotEmpty()) append(", $deltaArrow vs last week")
            }
        },
    ) {
        Text(
            text = value.toString(),
            style = MaterialTheme.typography.headlineSmall,
            fontWeight = FontWeight.Bold,
        )
        if (deltaArrow.isNotEmpty()) {
            Text(
                text = deltaArrow,
                style = MaterialTheme.typography.labelSmall,
                color = deltaColor,
            )
        }
        Text(
            text = label,
            style = MaterialTheme.typography.labelSmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
    }
}

// ─── Event row ────────────────────────────────────────────────────────────────

private val eventFormatter = DateTimeFormatter.ofPattern("EEE d MMM, HH:mm")
    .withZone(ZoneId.systemDefault())

@Composable
private fun EventRow(event: CalendarEvent, onClick: () -> Unit = {}) {
    Surface(
        shape = MaterialTheme.shapes.medium,
        tonalElevation = 1.dp,
        modifier = Modifier
            .fillMaxWidth()
            .semantics {
                contentDescription = "${event.summary}, ${eventFormatter.format(event.start)}"
            },
        onClick = onClick,
    ) {
        Row(
            modifier = Modifier.padding(12.dp),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Column(modifier = Modifier.weight(1f)) {
                Text(
                    text = event.summary,
                    style = MaterialTheme.typography.bodyMedium,
                    fontWeight = FontWeight.Medium,
                    maxLines = 1,
                )
                Text(
                    text = eventFormatter.format(event.start),
                    style = MaterialTheme.typography.labelSmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
        }
    }
}

// ─── Task row ─────────────────────────────────────────────────────────────────

private val taskDueFormatter = DateTimeFormatter.ofPattern("d MMM")
    .withZone(ZoneId.systemDefault())

@Composable
private fun TaskRow(task: CalDavTask, onClick: () -> Unit = {}) {
    val isOverdue = task.due != null && task.due.isBefore(java.time.Instant.now())
    Surface(
        shape = MaterialTheme.shapes.medium,
        tonalElevation = 1.dp,
        color = if (isOverdue) MaterialTheme.colorScheme.errorContainer.copy(alpha = 0.5f)
        else MaterialTheme.colorScheme.surface,
        modifier = Modifier
            .fillMaxWidth()
            .semantics {
                contentDescription = buildString {
                    append(task.summary)
                    if (task.due != null) append(", due ${taskDueFormatter.format(task.due)}")
                    if (isOverdue) append(", overdue")
                }
            },
        onClick = onClick,
    ) {
        Row(
            modifier = Modifier.padding(12.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.SpaceBetween,
        ) {
            Text(
                text = task.summary,
                style = MaterialTheme.typography.bodyMedium,
                fontWeight = FontWeight.Medium,
                maxLines = 1,
                modifier = Modifier.weight(1f),
            )
            if (task.due != null) {
                Text(
                    text = taskDueFormatter.format(task.due),
                    style = MaterialTheme.typography.labelSmall,
                    color = if (isOverdue) MaterialTheme.colorScheme.error
                    else MaterialTheme.colorScheme.onSurfaceVariant,
                    modifier = Modifier.padding(start = 8.dp),
                )
            }
        }
    }
}
