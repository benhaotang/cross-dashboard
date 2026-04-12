package com.crossdashboard.app.ui.screen.events

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.outlined.Refresh
import androidx.compose.material3.*
import androidx.compose.material3.adaptive.ExperimentalMaterial3AdaptiveApi
import androidx.compose.material3.adaptive.layout.AnimatedPane
import androidx.compose.material3.adaptive.layout.ListDetailPaneScaffoldRole
import androidx.compose.material3.adaptive.navigation.NavigableListDetailPaneScaffold
import androidx.compose.material3.adaptive.navigation.rememberListDetailPaneScaffoldNavigator
import androidx.compose.runtime.*
import kotlinx.coroutines.launch
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.hilt.navigation.compose.hiltViewModel
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.crossdashboard.app.domain.model.CalendarEvent
import com.crossdashboard.app.ui.component.CalendarColorDot
import com.crossdashboard.app.ui.component.CalendarColorResolver
import com.crossdashboard.app.ui.navigation.Destination
import java.time.ZoneId
import java.time.format.DateTimeFormatter
import java.time.temporal.ChronoUnit

@OptIn(ExperimentalMaterial3Api::class, ExperimentalMaterial3AdaptiveApi::class)
@Composable
fun EventsScreen(
    onNavigate: (Destination) -> Unit = {},
    colorResolver: CalendarColorResolver? = null,
    initialUid: String? = null,
    viewModel: EventsViewModel = hiltViewModel(),
) {
    val state by viewModel.state.collectAsStateWithLifecycle()
    val navigator = rememberListDetailPaneScaffoldNavigator<String>()
    var selectedEvent by remember { mutableStateOf<CalendarEvent?>(null) }
    val scope = rememberCoroutineScope()

    // When opened via a notification tap, auto-navigate the detail pane to the target event.
    // Runs whenever events load or the uid changes; the flag prevents double-navigation.
    var initialNavigationDone by remember(initialUid) { mutableStateOf(false) }
    LaunchedEffect(initialUid, state.events) {
        if (!initialNavigationDone && !initialUid.isNullOrEmpty() && state.events.isNotEmpty()) {
            val target = state.events.find { it.uid == initialUid }
            if (target != null) {
                selectedEvent = target
                navigator.navigateTo(ListDetailPaneScaffoldRole.Detail, target.uid)
                initialNavigationDone = true
            }
        }
    }

    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text("Events") },
                actions = {
                    if (state.isLoading) {
                        CircularProgressIndicator(
                            modifier = Modifier.size(20.dp).padding(end = 4.dp),
                            strokeWidth = 2.dp,
                        )
                    }
                    IconButton(
                        onClick = { viewModel.sync() },
                        enabled = !state.isLoading,
                        modifier = Modifier.semantics { contentDescription = "Sync events" },
                    ) {
                        Icon(Icons.Outlined.Refresh, contentDescription = null)
                    }
                },
            )
        },
    ) { paddingValues ->
        NavigableListDetailPaneScaffold(
            navigator = navigator,
            modifier = Modifier
                .fillMaxSize()
                .padding(paddingValues),
            listPane = {
                AnimatedPane {
                    Column(modifier = Modifier.fillMaxSize()) {
                        // ── Filter chips ──────────────────────────────────
                        Row(
                            modifier = Modifier
                                .fillMaxWidth()
                                .padding(horizontal = 12.dp, vertical = 4.dp),
                            horizontalArrangement = Arrangement.spacedBy(6.dp),
                        ) {
                            EventFilter.entries.forEach { filter ->
                                FilterChip(
                                    selected = state.filter == filter,
                                    onClick = { viewModel.setFilter(filter) },
                                    label = {
                                        Text(
                                            text = filter.name.lowercase().replaceFirstChar { it.uppercase() },
                                            style = MaterialTheme.typography.labelMedium,
                                        )
                                    },
                                )
                            }
                        }

                        if (state.events.isEmpty()) {
                            Box(Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
                                Text(
                                    "No events in this range",
                                    style = MaterialTheme.typography.bodyMedium,
                                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                                )
                            }
                        } else {
                            LazyColumn(
                                contentPadding = PaddingValues(bottom = 80.dp),
                                verticalArrangement = Arrangement.spacedBy(2.dp),
                            ) {
                                items(state.events, key = { it.uid }) { event ->
                                    EventListRow(
                                        event = event,
                                        colorResolver = colorResolver,
                        isSelected = navigator.currentDestination?.contentKey == event.uid,
                                    onClick = {
                                        selectedEvent = event
                                        scope.launch { navigator.navigateTo(ListDetailPaneScaffoldRole.Detail, event.uid) }
                                    },
                                    )
                                }
                            }
                        }
                    }
                }
            },
            detailPane = {
                AnimatedPane {
                    val event = selectedEvent
                    if (event != null) {
                        // Inline detail pane for tablet/Expanded — no bottom sheet
                        EventDetailContent(
                            event = event,
                            colorResolver = colorResolver,
                        )
                    } else {
                        Box(Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
                            Text(
                                "Select an event",
                                style = MaterialTheme.typography.bodyMedium,
                                color = MaterialTheme.colorScheme.onSurfaceVariant,
                            )
                        }
                    }
                }
            },
        )
    }

    // Phone: show bottom sheet when detail pane is active but scaffold is single-pane
    val selectedEventForSheet = if (navigator.scaffoldValue[ListDetailPaneScaffoldRole.Detail] ==
        androidx.compose.material3.adaptive.layout.PaneAdaptedValue.Hidden
    ) selectedEvent else null

    selectedEventForSheet?.let { event ->
        EventPropertySheet(
            event = event,
            colorResolver = colorResolver,
            onDismiss = { selectedEvent = null },
        )
    }
}

// ─── Event list row ───────────────────────────────────────────────────────────

private val dayFmt = DateTimeFormatter.ofPattern("EEE d MMM").withZone(ZoneId.systemDefault())
private val timeFmt = DateTimeFormatter.ofPattern("HH:mm").withZone(ZoneId.systemDefault())

@Composable
private fun EventListRow(
    event: CalendarEvent,
    colorResolver: CalendarColorResolver?,
    isSelected: Boolean = false,
    onClick: () -> Unit,
) {
    Surface(
        onClick = onClick,
        modifier = Modifier
            .fillMaxWidth()
            .semantics {
                contentDescription = buildString {
                    append("${event.summary}, ${dayFmt.format(event.start)}")
                    append(", ${timeFmt.format(event.start)} to ${timeFmt.format(event.end)}")
                    event.location?.let { append(", at $it") }
                }
            },
        color = if (isSelected) MaterialTheme.colorScheme.secondaryContainer.copy(alpha = 0.4f)
                else Color.Transparent,
    ) {
        Row(
            modifier = Modifier.padding(horizontal = 16.dp, vertical = 10.dp),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            // Calendar color dot
            if (colorResolver != null) {
                CalendarColorDot(
                    calendarHref = event.calendarHref,
                    resolver = colorResolver,
                    modifier = Modifier.padding(end = 10.dp),
                )
            }

            // Time column
            Column(
                horizontalAlignment = Alignment.CenterHorizontally,
                modifier = Modifier.width(48.dp),
            ) {
                Text(
                    text = timeFmt.format(event.start),
                    style = MaterialTheme.typography.labelLarge,
                    fontWeight = FontWeight.SemiBold,
                    color = MaterialTheme.colorScheme.primary,
                )
                Text(
                    text = timeFmt.format(event.end),
                    style = MaterialTheme.typography.labelSmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }

            Spacer(Modifier.width(12.dp))

            Column(modifier = Modifier.weight(1f)) {
                Text(
                    text = event.summary,
                    style = MaterialTheme.typography.bodyMedium,
                    fontWeight = FontWeight.Medium,
                    maxLines = 1,
                )
                val durationMins = java.time.Duration.between(event.start, event.end).toMinutes()
                val durationLabel = when {
                    durationMins < 60 -> "${durationMins}m"
                    durationMins % 60 == 0L -> "${durationMins / 60}h"
                    else -> "${durationMins / 60}h ${durationMins % 60}m"
                }
                Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    Text(
                        text = dayFmt.format(event.start),
                        style = MaterialTheme.typography.labelSmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                    Text(
                        text = "· $durationLabel",
                        style = MaterialTheme.typography.labelSmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                    event.location?.let {
                        Text(
                            text = "· $it",
                            style = MaterialTheme.typography.labelSmall,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                            maxLines = 1,
                        )
                    }
                }
            }
        }
    }

    HorizontalDivider(
        modifier = Modifier.padding(start = 76.dp),
        color = MaterialTheme.colorScheme.outlineVariant.copy(alpha = 0.4f),
        thickness = 0.5.dp,
    )
}
