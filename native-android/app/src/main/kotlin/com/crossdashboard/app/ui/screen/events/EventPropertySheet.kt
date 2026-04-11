package com.crossdashboard.app.ui.screen.events

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.compose.ui.text.style.TextOverflow
import com.crossdashboard.app.domain.model.CalendarEvent
import com.crossdashboard.app.ui.component.*
import java.time.ZoneId
import java.time.format.DateTimeFormatter

/**
 * Read-only detail sheet for calendar events (events cannot be edited in-app).
 */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun EventPropertySheet(
    event: CalendarEvent,
    colorResolver: CalendarColorResolver?,
    onDismiss: () -> Unit,
) {
    val sheetState = rememberModalBottomSheetState(skipPartiallyExpanded = true)

    PropertySheet(onDismiss = onDismiss, sheetState = sheetState) {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .verticalScroll(rememberScrollState())
                .navigationBarsPadding(),
        ) {
            PropertySheetHeader(
                title = event.summary,
                editing = false,
                onEditToggle = null,   // events are read-only
                onClose = onDismiss,
            )

            ReadField(label = "Start", value = dtFmt.format(event.start))
            ReadField(label = "End", value = dtFmt.format(event.end))

            val durationMins = java.time.Duration.between(event.start, event.end).toMinutes()
            val durationLabel = when {
                durationMins < 60 -> "${durationMins} min"
                durationMins % 60 == 0L -> "${durationMins / 60} h"
                else -> "${durationMins / 60}h ${durationMins % 60}m"
            }
            ReadField(label = "Duration", value = durationLabel)
            ReadField(label = "Location", value = event.location)
            ReadMarkdownField(label = "Description", value = event.description)
            ReadField(label = "UID", value = event.uid)

            colorResolver?.displayName(event.calendarHref)?.let { name ->
                ReadField(label = "Calendar", value = name)
            }

            Spacer(Modifier.height(16.dp))
        }
    }
}

/**
 * Inline detail pane content — shown in the detail pane of
 * [NavigableListDetailPaneScaffold] on Medium/Expanded windows.
 * Shares the same field layout as [EventPropertySheet] but without the modal wrapper.
 */
@Composable
fun EventDetailContent(
    event: CalendarEvent,
    colorResolver: CalendarColorResolver?,
    modifier: Modifier = Modifier,
) {
    Column(
        modifier = modifier
            .fillMaxSize()
            .verticalScroll(rememberScrollState())
            .padding(horizontal = 16.dp, vertical = 12.dp),
        verticalArrangement = Arrangement.spacedBy(4.dp),
    ) {
        Text(event.summary, style = MaterialTheme.typography.headlineSmall)
        Spacer(Modifier.height(8.dp))
        ReadField(label = "Start", value = dtFmt.format(event.start))
        ReadField(label = "End", value = dtFmt.format(event.end))
        val durationMins = java.time.Duration.between(event.start, event.end).toMinutes()
        val durationLabel = when {
            durationMins < 60 -> "${durationMins} min"
            durationMins % 60 == 0L -> "${durationMins / 60} h"
            else -> "${durationMins / 60}h ${durationMins % 60}m"
        }
        ReadField(label = "Duration", value = durationLabel)
        ReadField(label = "Location", value = event.location)
        ReadMarkdownField(label = "Description", value = event.description)
        ReadField(label = "UID", value = event.uid)
        colorResolver?.displayName(event.calendarHref)?.let { name ->
            ReadField(label = "Calendar", value = name)
        }
    }
}

private val dtFmt = DateTimeFormatter.ofPattern("EEE d MMM yyyy, HH:mm")
    .withZone(ZoneId.systemDefault())
