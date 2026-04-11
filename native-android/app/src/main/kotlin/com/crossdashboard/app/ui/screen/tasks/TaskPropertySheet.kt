package com.crossdashboard.app.ui.screen.tasks

import androidx.compose.animation.ExperimentalSharedTransitionApi
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.outlined.CalendarToday
import androidx.compose.material.icons.outlined.Clear
import androidx.compose.material.icons.outlined.Schedule
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
import com.crossdashboard.app.domain.model.TaskStatus
import com.crossdashboard.app.ui.component.*
import com.crossdashboard.app.ui.screen.views.CoveyTag
import java.time.Instant
import java.time.LocalDate
import java.time.ZoneId
import java.time.ZoneOffset
import java.time.format.DateTimeFormatter

@OptIn(ExperimentalMaterial3Api::class, ExperimentalSharedTransitionApi::class)
@Composable
fun TaskPropertySheet(
    task: CalDavTask,
    kanbanColumns: List<String>,
    colorResolver: CalendarColorResolver?,
    onDismiss: () -> Unit,
    onSave: (CalDavTask) -> Unit,
    onDelete: () -> Unit,
    onToggleComplete: () -> Unit,
    /** When true, renders content inline (no ModalBottomSheet wrapper) for tablet detail pane. */
    inlineMode: Boolean = false,
    pomodoroVm: PomodoroViewModel = hiltViewModel(),
) {
    val sheetState = rememberModalBottomSheetState(skipPartiallyExpanded = true)
    var editing by remember { mutableStateOf(false) }
    var editedTask by remember { mutableStateOf(task) }

    // Subtasks from DB
    val tasksVm: TasksViewModel = hiltViewModel()
    val subtasks by tasksVm.subtasksOf(task.uid).collectAsStateWithLifecycle(emptyList())

    // In inline (tablet) mode apply sharedBounds so the selected task card morphs into this pane.
    val sharedTransScope = LocalSharedTransitionScope.current
    val animVisScope = LocalAnimVisibilityScope.current
    val sharedContentMod: Modifier = if (inlineMode && sharedTransScope != null && animVisScope != null) {
        with(sharedTransScope) {
            Modifier.sharedBounds(
                rememberSharedContentState(key = "task_card_${task.uid}"),
                animatedVisibilityScope = animVisScope,
            )
        }
    } else Modifier

    @Composable
    fun SheetContent() {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .verticalScroll(rememberScrollState())
                .then(if (!inlineMode) Modifier.navigationBarsPadding() else Modifier)
                .then(sharedContentMod),
        ) {
            PropertySheetHeader(
                title = if (editing) "Edit Task" else task.summary,
                editing = editing,
                onEditToggle = {
                    if (editing) {
                        editedTask = task  // cancel: reset
                    }
                    editing = !editing
                },
                onClose = onDismiss,
            )

            if (editing) {
                TaskEditForm(
                    task = editedTask,
                    kanbanColumns = kanbanColumns,
                    onTaskChange = { editedTask = it },
                    onSave = {
                        onSave(editedTask)
                        editing = false
                    },
                    onDelete = onDelete,
                )
            } else {
                TaskReadView(
                    task = task,
                    subtasks = subtasks,
                    kanbanColumns = kanbanColumns,
                    colorResolver = colorResolver,
                    onToggleComplete = onToggleComplete,
                    onSubtaskToggle = { tasksVm.toggleComplete(it) },
                    onPomodoroStart = {
                        pomodoroVm.start(task.summary) {
                            val log = "[Pomodoro ${java.time.LocalDateTime.now(ZoneId.systemDefault())
                                .format(DateTimeFormatter.ofPattern("MM-dd HH:mm"))}]"
                            val updatedDesc = if (task.description.isNullOrBlank()) log
                            else "${task.description}\n$log"
                            tasksVm.saveTask(task.copy(description = updatedDesc))
                        }
                    },
                )
            }
        }
    }

    // Inline mode: render content directly in the detail pane (tablet).
    // Normal mode: wrap in ModalBottomSheet (phone).
    if (inlineMode) {
        SheetContent()
    } else {
        PropertySheet(onDismiss = onDismiss, sheetState = sheetState) {
            SheetContent()
        }
    }
}

// ─── Read view ────────────────────────────────────────────────────────────────

private val dtFmt = DateTimeFormatter.ofPattern("EEE d MMM yyyy, HH:mm")
    .withZone(ZoneId.systemDefault())
private val dateFmt = DateTimeFormatter.ofPattern("EEE d MMM yyyy")
    .withZone(ZoneId.systemDefault())

@Composable
private fun TaskReadView(
    task: CalDavTask,
    subtasks: List<CalDavTask>,
    kanbanColumns: List<String>,
    colorResolver: CalendarColorResolver?,
    onToggleComplete: () -> Unit,
    onSubtaskToggle: (CalDavTask) -> Unit,
    onPomodoroStart: () -> Unit,
) {
    // Status badge + priority + Pomodoro button
    ChipRow {
        val (statusColor, statusOnColor) = when (task.status) {
            TaskStatus.NEEDS_ACTION -> MaterialTheme.colorScheme.surfaceVariant to MaterialTheme.colorScheme.onSurfaceVariant
            TaskStatus.IN_PROCESS -> MaterialTheme.colorScheme.primaryContainer to MaterialTheme.colorScheme.onPrimaryContainer
            TaskStatus.COMPLETED -> MaterialTheme.colorScheme.tertiaryContainer to MaterialTheme.colorScheme.onTertiaryContainer
            TaskStatus.CANCELLED -> MaterialTheme.colorScheme.errorContainer to MaterialTheme.colorScheme.onErrorContainer
        }
        StatusBadge(
            label = task.status.name.replace('_', ' '),
            containerColor = statusColor,
            contentColor = statusOnColor,
        )
        PriorityChip(priority = task.priority)
        Spacer(Modifier.weight(1f))
        // Pomodoro play button
        TextButton(onClick = onPomodoroStart) {
            Text("▶  Pomodoro", style = MaterialTheme.typography.labelMedium)
        }
    }

    // Progress bar
    if (task.percentComplete > 0) {
        Column(modifier = Modifier.padding(horizontal = 20.dp, vertical = 8.dp)) {
            Text(
                text = "Progress — ${task.percentComplete}%",
                style = MaterialTheme.typography.labelSmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
            Spacer(Modifier.height(4.dp))
            LinearProgressIndicator(
                progress = { task.percentComplete / 100f },
                modifier = Modifier.fillMaxWidth(),
            )
        }
    }

    ReadField(label = "Due", value = task.due?.let { dtFmt.format(it) })
    ReadField(label = "Start", value = task.dtstart?.let { dateFmt.format(it) })
    ReadMarkdownField(label = "Description", value = task.description)
    ReadField(label = "Location", value = task.location)

    // Calendar
    colorResolver?.displayName(task.calendarHref)?.let { name ->
        ReadField(label = "Calendar", value = name)
    }

    // Categories
    if (task.categories.isNotEmpty()) {
        SheetSectionHeader(title = "Tags")
        ChipRow {
            task.categories.forEach { tag ->
                TagChip(label = "#$tag")
            }
        }
    }

    // Kanban quick-tags
    val effectiveColumns = kanbanColumns.ifEmpty {
        listOf("backlog", "planned", "inprogress", "done")
    }
    SheetSectionHeader(title = "Kanban")
    ChipRow {
        effectiveColumns.forEach { col ->
            TagChip(
                label = col,
                selected = col in task.categories,
                onClick = {}, // read-only in this view; edit mode handles mutation
            )
        }
    }

    // Covey quadrant quick-tags
    SheetSectionHeader(title = "Quadrant")
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .horizontalScroll(rememberScrollState())
            .padding(horizontal = 20.dp)
            .padding(vertical = 4.dp),
        horizontalArrangement = Arrangement.spacedBy(6.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        listOf(
            CoveyTag.DO to "Do",
            CoveyTag.DELAY to "Delay",
            CoveyTag.DELEGATE to "Delegate",
            CoveyTag.ELIMINATE to "Eliminate",
        ).forEach { (tag, label) ->
            TagChip(
                label = label,
                selected = tag in task.categories,
                onClick = {}, // read-only; edit mode handles mutation
            )
        }
    }

    // Subtasks
    if (subtasks.isNotEmpty()) {
        SheetSectionHeader(title = "Subtasks")
        subtasks.forEach { sub ->
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(horizontal = 20.dp, vertical = 4.dp),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Checkbox(
                    checked = sub.status == TaskStatus.COMPLETED,
                    onCheckedChange = { onSubtaskToggle(sub) },
                    modifier = Modifier.size(20.dp),
                )
                Spacer(Modifier.width(8.dp))
                Text(
                    text = sub.summary,
                    style = MaterialTheme.typography.bodySmall,
                    color = if (sub.status == TaskStatus.COMPLETED)
                        MaterialTheme.colorScheme.onSurfaceVariant
                    else MaterialTheme.colorScheme.onSurface,
                )
            }
        }
    }

    // Complete / reopen button
    Spacer(Modifier.height(16.dp))
    Button(
        onClick = onToggleComplete,
        modifier = Modifier
            .fillMaxWidth()
            .padding(horizontal = 20.dp),
        colors = ButtonDefaults.buttonColors(
            containerColor = if (task.status == TaskStatus.COMPLETED)
                MaterialTheme.colorScheme.surfaceVariant
            else MaterialTheme.colorScheme.primary,
        ),
    ) {
        Text(
            text = if (task.status == TaskStatus.COMPLETED) "Reopen" else "Mark Complete",
        )
    }
}

// ─── Edit form ────────────────────────────────────────────────────────────────

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun TaskEditForm(
    task: CalDavTask,
    kanbanColumns: List<String>,
    onTaskChange: (CalDavTask) -> Unit,
    onSave: () -> Unit,
    onDelete: () -> Unit,
) {
    val zone = ZoneId.systemDefault()
    var showDeleteDialog by remember { mutableStateOf(false) }
    var showDatePicker by remember { mutableStateOf(false) }
    var showTimePicker by remember { mutableStateOf(false) }

    Column(
        modifier = Modifier.padding(horizontal = 20.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp),
    ) {
        OutlinedTextField(
            value = task.summary,
            onValueChange = { onTaskChange(task.copy(summary = it)) },
            label = { Text("Title") },
            modifier = Modifier.fillMaxWidth(),
        )

        OutlinedTextField(
            value = task.description ?: "",
            onValueChange = { onTaskChange(task.copy(description = it.ifBlank { null })) },
            label = { Text("Description") },
            modifier = Modifier.fillMaxWidth(),
            minLines = 3,
            maxLines = 6,
        )

        // ── Due date / time ───────────────────────────────────────────────────
        Text(
            text = "Due",
            style = MaterialTheme.typography.labelMedium,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
        Row(
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(4.dp),
        ) {
            val dueZoned = task.due?.atZone(zone)
            // Date button
            OutlinedButton(
                onClick = { showDatePicker = true },
                modifier = Modifier
                    .weight(1f)
                    .semantics {
                        contentDescription = if (dueZoned != null)
                            "Due date: ${dueZoned.format(DateTimeFormatter.ofPattern("d MMM yyyy"))}, tap to change"
                        else "No due date, tap to set"
                    },
            ) {
                Icon(Icons.Outlined.CalendarToday, contentDescription = null, modifier = Modifier.size(16.dp))
                Spacer(Modifier.width(6.dp))
                Text(
                    text = dueZoned?.format(DateTimeFormatter.ofPattern("d MMM yyyy")) ?: "Set date",
                    style = MaterialTheme.typography.labelMedium,
                )
            }
            // Time button — only shown once a date is chosen
            if (dueZoned != null) {
                OutlinedButton(
                    onClick = { showTimePicker = true },
                    modifier = Modifier.semantics {
                        contentDescription = "Due time: ${dueZoned.format(DateTimeFormatter.ofPattern("HH:mm"))}, tap to change"
                    },
                ) {
                    Icon(Icons.Outlined.Schedule, contentDescription = null, modifier = Modifier.size(16.dp))
                    Spacer(Modifier.width(6.dp))
                    Text(
                        text = dueZoned.format(DateTimeFormatter.ofPattern("HH:mm")),
                        style = MaterialTheme.typography.labelMedium,
                    )
                }
                // Clear due date
                IconButton(
                    onClick = { onTaskChange(task.copy(due = null)) },
                    modifier = Modifier.semantics { contentDescription = "Clear due date" },
                ) {
                    Icon(Icons.Outlined.Clear, contentDescription = null)
                }
            }
        }

        // ── Priority selector ─────────────────────────────────────────────────
        Text(
            text = "Priority",
            style = MaterialTheme.typography.labelMedium,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
        Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            listOf(0 to "None", 1 to "High", 5 to "Medium", 9 to "Low").forEach { (value, label) ->
                FilterChip(
                    selected = when (value) {
                        0 -> task.priority == 0
                        1 -> task.priority in 1..4
                        5 -> task.priority == 5
                        9 -> task.priority in 6..9
                        else -> false
                    },
                    onClick = { onTaskChange(task.copy(priority = value)) },
                    label = { Text(label, style = MaterialTheme.typography.labelSmall) },
                )
            }
        }

        // ── Kanban quick-tags ─────────────────────────────────────────────────
        val effectiveColumns = kanbanColumns.ifEmpty {
            listOf("backlog", "planned", "inprogress", "done")
        }
        Text(
            text = "Kanban",
            style = MaterialTheme.typography.labelMedium,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
        Row(
            horizontalArrangement = Arrangement.spacedBy(6.dp),
            modifier = Modifier
                .fillMaxWidth()
                .horizontalScroll(rememberScrollState()),
        ) {
            effectiveColumns.forEach { col ->
                val isSelected = col in task.categories
                FilterChip(
                    selected = isSelected,
                    onClick = {
                        val updated = if (isSelected) {
                            task.categories - col
                        } else {
                            // Mutual exclusivity within kanban columns
                            (task.categories - effectiveColumns.toSet()) + col
                        }
                        onTaskChange(task.copy(categories = updated))
                    },
                    label = { Text(col, style = MaterialTheme.typography.labelSmall) },
                )
            }
        }

        // ── Covey quadrant tags ───────────────────────────────────────────────
        Text(
            text = "Quadrant",
            style = MaterialTheme.typography.labelMedium,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
        Row(
            horizontalArrangement = Arrangement.spacedBy(6.dp),
            modifier = Modifier
                .fillMaxWidth()
                .horizontalScroll(rememberScrollState()),
        ) {
            listOf(
                CoveyTag.DO to "Do",
                CoveyTag.DELAY to "Delay",
                CoveyTag.DELEGATE to "Delegate",
                CoveyTag.ELIMINATE to "Eliminate",
            ).forEach { (tag, label) ->
                val isSelected = tag in task.categories
                FilterChip(
                    selected = isSelected,
                    onClick = {
                        val updated = if (isSelected) {
                            task.categories - tag
                        } else {
                            // Mutual exclusivity within quadrant tags
                            (task.categories - CoveyTag.ALL.toSet()) + tag
                        }
                        onTaskChange(task.copy(categories = updated))
                    },
                    label = { Text(label, style = MaterialTheme.typography.labelSmall) },
                )
            }
        }

        OutlinedTextField(
            value = task.location ?: "",
            onValueChange = { onTaskChange(task.copy(location = it.ifBlank { null })) },
            label = { Text("Location") },
            modifier = Modifier.fillMaxWidth(),
        )

        Spacer(Modifier.height(8.dp))

        Button(onClick = onSave, modifier = Modifier.fillMaxWidth()) {
            Text("Save")
        }

        OutlinedButton(
            onClick = { showDeleteDialog = true },
            modifier = Modifier.fillMaxWidth(),
            colors = ButtonDefaults.outlinedButtonColors(
                contentColor = MaterialTheme.colorScheme.error,
            ),
        ) {
            Text("Delete Task")
        }
    }

    // ── Date picker dialog ────────────────────────────────────────────────────
    if (showDatePicker) {
        // Convert existing due Instant → UTC-midnight millis for the date picker
        val initialMillis = task.due
            ?.atZone(zone)?.toLocalDate()
            ?.atStartOfDay(ZoneOffset.UTC)?.toInstant()?.toEpochMilli()
            ?: System.currentTimeMillis()
        val datePickerState = rememberDatePickerState(initialSelectedDateMillis = initialMillis)
        DatePickerDialog(
            onDismissRequest = { showDatePicker = false },
            confirmButton = {
                TextButton(onClick = {
                    showDatePicker = false
                    datePickerState.selectedDateMillis?.let { millis ->
                        // Interpret the picker's UTC-date millis as a local date
                        val pickedDate = LocalDate.ofInstant(Instant.ofEpochMilli(millis), ZoneOffset.UTC)
                        // Preserve the existing time-of-day if available, else default to 09:00
                        val existingZoned = task.due?.atZone(zone)
                        val hour = existingZoned?.hour ?: 9
                        val minute = existingZoned?.minute ?: 0
                        val newInstant = pickedDate.atTime(hour, minute)
                            .atZone(zone).toInstant()
                        onTaskChange(task.copy(due = newInstant))
                        showTimePicker = true
                    }
                }) { Text("Next") }
            },
            dismissButton = {
                TextButton(onClick = { showDatePicker = false }) { Text("Cancel") }
            },
        ) {
            DatePicker(state = datePickerState)
        }
    }

    // ── Time picker dialog ────────────────────────────────────────────────────
    if (showTimePicker) {
        val dueZoned = task.due?.atZone(zone)
        val timePickerState = rememberTimePickerState(
            initialHour = dueZoned?.hour ?: 9,
            initialMinute = dueZoned?.minute ?: 0,
            is24Hour = true,
        )
        AlertDialog(
            onDismissRequest = { showTimePicker = false },
            title = { Text("Set time") },
            text = {
                Box(contentAlignment = Alignment.Center, modifier = Modifier.fillMaxWidth()) {
                    TimePicker(state = timePickerState)
                }
            },
            confirmButton = {
                TextButton(onClick = {
                    showTimePicker = false
                    val baseDate = task.due?.atZone(zone)?.toLocalDate()
                        ?: LocalDate.now(zone)
                    val newInstant = baseDate
                        .atTime(timePickerState.hour, timePickerState.minute)
                        .atZone(zone).toInstant()
                    onTaskChange(task.copy(due = newInstant))
                }) { Text("OK") }
            },
            dismissButton = {
                TextButton(onClick = { showTimePicker = false }) { Text("Cancel") }
            },
        )
    }

    // ── Delete confirmation dialog ────────────────────────────────────────────
    if (showDeleteDialog) {
        AlertDialog(
            onDismissRequest = { showDeleteDialog = false },
            title = { Text("Delete Task") },
            text = { Text("Delete \"${task.summary}\"? This cannot be undone.") },
            confirmButton = {
                TextButton(onClick = {
                    showDeleteDialog = false
                    onDelete()
                }) { Text("Delete", color = MaterialTheme.colorScheme.error) }
            },
            dismissButton = {
                TextButton(onClick = { showDeleteDialog = false }) { Text("Cancel") }
            },
        )
    }
}
