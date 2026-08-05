@file:OptIn(androidx.compose.animation.ExperimentalSharedTransitionApi::class)

package com.crossdashboard.app.ui.screen.tasks

import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.ExperimentalSharedTransitionApi
import androidx.compose.animation.SharedTransitionLayout
import androidx.compose.animation.SharedTransitionScope
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.text.KeyboardActions
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.outlined.Add
import androidx.compose.material.icons.outlined.Refresh
import androidx.compose.material3.*
import androidx.compose.material3.adaptive.ExperimentalMaterial3AdaptiveApi
import androidx.compose.material3.adaptive.layout.AnimatedPane
import androidx.compose.material3.adaptive.layout.ListDetailPaneScaffoldRole
import androidx.compose.material3.adaptive.layout.PaneAdaptedValue
import androidx.compose.material3.adaptive.navigation.NavigableListDetailPaneScaffold
import androidx.compose.material3.adaptive.navigation.rememberListDetailPaneScaffoldNavigator
import androidx.compose.runtime.*
import kotlinx.coroutines.launch
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.focus.FocusRequester
import androidx.compose.ui.focus.focusRequester
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.semantics.stateDescription
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.unit.dp
import androidx.hilt.navigation.compose.hiltViewModel
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.crossdashboard.app.domain.model.CalDavTask
import com.crossdashboard.app.domain.model.TaskStatus
import com.crossdashboard.app.ui.component.CalendarColorDot
import com.crossdashboard.app.ui.component.CalendarColorResolver
import com.crossdashboard.app.ui.component.AdaptiveFilterBar
import com.crossdashboard.app.ui.component.AdaptiveFilterSpec
import com.crossdashboard.app.ui.component.FilterChoice
import com.crossdashboard.app.ui.component.PriorityChip
import com.crossdashboard.app.ui.component.TagFlow
import com.crossdashboard.app.ui.navigation.Destination
import kotlinx.coroutines.delay
import java.time.ZoneId
import java.time.format.DateTimeFormatter

// CompositionLocals that thread SharedTransitionLayout's scope + the current AnimatedPane's
// AnimatedVisibilityScope down to TaskRow and TaskPropertySheet without prop-drilling.
internal val LocalSharedTransitionScope =
    compositionLocalOf<SharedTransitionScope?> { null }
internal val LocalAnimVisibilityScope =
    compositionLocalOf<androidx.compose.animation.AnimatedVisibilityScope?> { null }

/**
 * Entry point for the Tasks screen.
 * [colorResolver] is injected from the navigation host via [NavigationViewModel]
 * to avoid repeated Hilt lookups at each composition level.
 */
@OptIn(
    ExperimentalMaterial3Api::class,
    ExperimentalMaterial3AdaptiveApi::class,
    ExperimentalSharedTransitionApi::class,
)
@Composable
fun TasksScreenContent(
    onNavigate: (Destination) -> Unit = {},
    pendingAction: String? = null,
    colorResolver: CalendarColorResolver? = null,
    initialUid: String? = null,
    viewModel: TasksViewModel = hiltViewModel(),
) {
    val state by viewModel.state.collectAsStateWithLifecycle()
    val focusRequester = remember { FocusRequester() }
    val navigator = rememberListDetailPaneScaffoldNavigator<String>()
    var selectedTask by remember { mutableStateOf<CalDavTask?>(null) }
    val scope = rememberCoroutineScope()

    fun openTask(task: CalDavTask) {
        selectedTask = task
        if (navigator.scaffoldValue[ListDetailPaneScaffoldRole.Detail] != PaneAdaptedValue.Hidden) {
            scope.launch { navigator.navigateTo(ListDetailPaneScaffoldRole.Detail, task.uid) }
        }
    }

    // Handle deep link auto-focus (widget add-task shortcut)
    LaunchedEffect(pendingAction) {
        if (pendingAction == "add_task") {
            viewModel.setAutoFocus(true)
            delay(300)
            runCatching { focusRequester.requestFocus() }
        }
    }

    // Handle task-reminder notification tap: auto-open the detail pane for the target task
    var initialNavigationDone by remember(initialUid) { mutableStateOf(false) }
    LaunchedEffect(initialUid, state.tasks) {
        if (!initialNavigationDone && !initialUid.isNullOrEmpty() && state.tasks.isNotEmpty()) {
            val target = state.tasks.find { it.uid == initialUid }
            if (target != null) {
                selectedTask = target
                if (navigator.scaffoldValue[ListDetailPaneScaffoldRole.Detail] != PaneAdaptedValue.Hidden) {
                    navigator.navigateTo(ListDetailPaneScaffoldRole.Detail, target.uid)
                }
                initialNavigationDone = true
            }
        }
    }

    Scaffold(
        topBar = {
            TasksTopBar(
                onRefresh = { viewModel.sync() },
                isLoading = state.isLoading,
            )
        },
    ) { paddingValues ->
        // SharedTransitionLayout provides SharedTransitionScope to descendant task rows
        // and the detail pane so cards can animate into the detail sheet on tablet.
        SharedTransitionLayout {
            NavigableListDetailPaneScaffold(
                navigator = navigator,
                modifier = Modifier
                    .fillMaxSize()
                    .padding(paddingValues),
                listPane = {
                    AnimatedPane {
                        // Provide both scopes via CompositionLocals so TaskRow can
                        // reference them without explicit parameter passing.
                        CompositionLocalProvider(
                            LocalSharedTransitionScope provides this@SharedTransitionLayout,
                            LocalAnimVisibilityScope provides this,
                        ) {
                            Column(modifier = Modifier.fillMaxSize()) {
                                // ── Filter chips ────────────────────────────
                                FilterRow(
                                    state = state,
                                    onSelectStatus = viewModel::setFilter,
                                    onSelectTags = viewModel::setTagFilters,
                                    onClear = viewModel::clearFilters,
                                )

                                // ── Quick input bar ─────────────────────────
                                QuickInputBar(
                                    input = state.quickInput,
                                    onInputChange = viewModel::onQuickInputChange,
                                    onSubmit = viewModel::submitQuickInput,
                                    parsedPreview = state.parsedPreview,
                                    focusRequester = focusRequester,
                                    modifier = Modifier.padding(horizontal = 12.dp, vertical = 6.dp),
                                )

                                // ── Error snackbar ───────────────────────────
                                state.error?.let { err ->
                                    Snackbar(
                                        modifier = Modifier.padding(horizontal = 12.dp),
                                        action = {
                                            TextButton(onClick = viewModel::dismissError) { Text("Dismiss") }
                                        },
                                    ) { Text(err) }
                                }

                                // ── Task list ────────────────────────────────
                                val selectedUid = navigator.currentDestination?.contentKey
                                if (state.rootTasks.isEmpty()) {
                                    Box(Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
                                        Text(
                                            text = when (state.filter) {
                                                TaskFilter.ALL -> "No tasks"
                                                TaskFilter.ACTIVE -> "No active tasks"
                                                TaskFilter.COMPLETED -> "No completed tasks"
                                            },
                                            style = MaterialTheme.typography.bodyMedium,
                                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                                        )
                                    }
                                } else {
                                    LazyColumn(
                                        contentPadding = PaddingValues(bottom = 100.dp),
                                        verticalArrangement = Arrangement.spacedBy(2.dp),
                                    ) {
                                        items(state.rootTasks, key = { it.uid }) { task ->
                                            TaskTreeNode(
                                                task = task,
                                                depth = 0,
                                                expanded = task.uid in state.expandedUids,
                                                onExpandToggle = { viewModel.toggleExpand(task.uid) },
                                                onTaskClick = { openTask(task) },
                                                onToggleComplete = { viewModel.toggleComplete(task) },
                                                colorResolver = colorResolver,
                                                subtasksFlow = viewModel.subtasksOf(task.uid),
                                                expandedUids = state.expandedUids,
                                                onSubtaskClick = { openTask(it) },
                                                onSubtaskToggle = { viewModel.toggleComplete(it) },
                                                onSubtaskExpand = { viewModel.toggleExpand(it.uid) },
                                                magicTags = state.kanbanColumns,
                                                isSelected = task.uid == selectedUid,
                                            )
                                        }
                                    }
                                }
                            }
                        }
                    }
                },
                detailPane = {
                    AnimatedPane {
                        CompositionLocalProvider(
                            LocalSharedTransitionScope provides this@SharedTransitionLayout,
                            LocalAnimVisibilityScope provides this,
                        ) {
                        val task = selectedTask
                        if (task != null) {
                            TaskPropertySheet(
                                task = task,
                                kanbanColumns = state.kanbanColumns,
                                colorResolver = colorResolver,
                                inlineMode = true,
                                onDismiss = { scope.launch { navigator.navigateBack() } },
                                onSave = { updated -> viewModel.saveTask(updated) },
                                onDelete = { viewModel.deleteTask(task); scope.launch { navigator.navigateBack() } },
                                onToggleComplete = { viewModel.toggleComplete(task) },
                            )
                            } else {
                                Box(Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
                                    Text(
                                        "Select a task",
                                        style = MaterialTheme.typography.bodyMedium,
                                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                                    )
                                }
                            }
                        }
                    }
                },
            )
        }
    }

    // Phone: bottom sheet when detail pane is hidden (single-pane mode)
    val taskForSheet = if (navigator.scaffoldValue[ListDetailPaneScaffoldRole.Detail] ==
        PaneAdaptedValue.Hidden
    ) selectedTask else null

    taskForSheet?.let { task ->
        TaskPropertySheet(
            task = task,
            kanbanColumns = state.kanbanColumns,
            colorResolver = colorResolver,
            inlineMode = false,
            onDismiss = { selectedTask = null },
            onSave = { updated ->
                viewModel.saveTask(updated)
                selectedTask = null
            },
            onDelete = {
                viewModel.deleteTask(task)
                selectedTask = null
            },
            onToggleComplete = {
                viewModel.toggleComplete(task)
                selectedTask = null
            },
        )
    }
}

// ─── Top bar ──────────────────────────────────────────────────────────────────

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun TasksTopBar(onRefresh: () -> Unit, isLoading: Boolean = false) {
    TopAppBar(
        title = { Text("Tasks") },
        actions = {
            if (isLoading) {
                CircularProgressIndicator(
                    modifier = Modifier.size(20.dp).padding(end = 4.dp),
                    strokeWidth = 2.dp,
                )
            }
            IconButton(
                onClick = onRefresh,
                enabled = !isLoading,
                modifier = Modifier.semantics { contentDescription = "Sync tasks" },
            ) {
                Icon(Icons.Outlined.Refresh, contentDescription = null)
            }
        },
    )
}

// ─── Filter chips ─────────────────────────────────────────────────────────────

@Composable
private fun FilterRow(
    state: TasksUiState,
    onSelectStatus: (TaskFilter) -> Unit,
    onSelectTags: (Set<String>) -> Unit,
    onClear: () -> Unit,
) {
    AdaptiveFilterBar(
        filters = listOf(
            AdaptiveFilterSpec(
                title = "Status",
                choices = TaskFilter.entries.map {
                    FilterChoice(it.name, it.name.lowercase().replaceFirstChar { char -> char.uppercase() })
                },
                selectedKeys = setOf(state.filter.name),
                onSelectionChange = { selected ->
                    selected.firstOrNull()?.let { onSelectStatus(TaskFilter.valueOf(it)) }
                },
            ),
            AdaptiveFilterSpec(
                title = "Tags",
                choices = state.availableTags.map { FilterChoice(it, "#$it") },
                selectedKeys = state.selectedTags,
                multiSelect = true,
                searchable = true,
                onSelectionChange = onSelectTags,
            ),
        ),
        hasActiveFilters = state.filter != TaskFilter.ACTIVE || state.selectedTags.isNotEmpty(),
        onClear = onClear,
    )
}

// ─── Quick input bar ──────────────────────────────────────────────────────────

@Composable
private fun QuickInputBar(
    input: String,
    onInputChange: (String) -> Unit,
    onSubmit: () -> Unit,
    parsedPreview: com.crossdashboard.app.domain.model.ParsedTask?,
    focusRequester: FocusRequester,
    modifier: Modifier = Modifier,
) {
    Column(modifier = modifier) {
        OutlinedTextField(
            value = input,
            onValueChange = onInputChange,
            label = { Text("Quick add task") },
            placeholder = { Text("!! #tag tomorrow") },
            singleLine = true,
            keyboardOptions = KeyboardOptions(imeAction = ImeAction.Done),
            keyboardActions = KeyboardActions(onDone = { onSubmit() }),
            trailingIcon = {
                if (input.isNotBlank()) {
                    IconButton(
                        onClick = onSubmit,
                        modifier = Modifier.semantics { contentDescription = "Add task" },
                    ) {
                        Icon(Icons.Outlined.Add, contentDescription = null)
                    }
                }
            },
            modifier = Modifier
                .fillMaxWidth()
                .focusRequester(focusRequester),
        )

        // Live parse preview chips
        if (parsedPreview != null && input.isNotBlank()) {
            Column(modifier = Modifier.padding(top = 4.dp)) {
                Row(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                    if (parsedPreview.priority > 0) {
                        PriorityChip(priority = parsedPreview.priority)
                    }
                    parsedPreview.due?.let { due ->
                        val fmt = DateTimeFormatter.ofPattern("d MMM HH:mm").withZone(ZoneId.systemDefault())
                        SuggestionChip(
                            onClick = {},
                            label = {
                                Text(fmt.format(due), style = MaterialTheme.typography.labelSmall)
                            },
                            modifier = Modifier.semantics { contentDescription = "Due: ${fmt.format(due)}" },
                        )
                    }
                }
                if (parsedPreview.categories.isNotEmpty()) {
                    TagFlow(
                        tags = parsedPreview.categories,
                        modifier = Modifier.padding(top = 4.dp),
                    )
                }
            }
        }
    }
}

// ─── Task tree node ───────────────────────────────────────────────────────────

private val dueFmt = DateTimeFormatter.ofPattern("d MMM").withZone(ZoneId.systemDefault())

@Composable
private fun TaskTreeNode(
    task: CalDavTask,
    depth: Int,
    expanded: Boolean,
    onExpandToggle: () -> Unit,
    onTaskClick: () -> Unit,
    onToggleComplete: () -> Unit,
    colorResolver: CalendarColorResolver?,
    subtasksFlow: kotlinx.coroutines.flow.Flow<List<CalDavTask>>,
    expandedUids: Set<String>,
    onSubtaskClick: (CalDavTask) -> Unit,
    onSubtaskToggle: (CalDavTask) -> Unit,
    onSubtaskExpand: (CalDavTask) -> Unit,
    magicTags: List<String>,
    isSelected: Boolean = false,
) {
    val subtasks by subtasksFlow.collectAsStateWithLifecycle(emptyList())
    val hasChildren = subtasks.isNotEmpty()

    TaskRow(
        task = task,
        depth = depth,
        hasChildren = hasChildren,
        subtaskCount = subtasks.size,
        expanded = expanded,
        onExpandToggle = onExpandToggle,
        onClick = onTaskClick,
        onToggleComplete = onToggleComplete,
        colorResolver = colorResolver,
        magicTags = magicTags,
        isSelected = isSelected,
    )

    AnimatedVisibility(visible = expanded && hasChildren) {
        Column {
            subtasks.forEach { child ->
                TaskRow(
                    task = child,
                    depth = depth + 1,
                    hasChildren = false,
                    subtaskCount = 0,
                    expanded = false,
                    onExpandToggle = { onSubtaskExpand(child) },
                    onClick = { onSubtaskClick(child) },
                    onToggleComplete = { onSubtaskToggle(child) },
                    colorResolver = colorResolver,
                    magicTags = magicTags,
                )
            }
        }
    }
}

@OptIn(ExperimentalSharedTransitionApi::class)
@Composable
private fun TaskRow(
    task: CalDavTask,
    depth: Int,
    hasChildren: Boolean,
    subtaskCount: Int,
    expanded: Boolean,
    onExpandToggle: () -> Unit,
    onClick: () -> Unit,
    onToggleComplete: () -> Unit,
    colorResolver: CalendarColorResolver?,
    magicTags: List<String>,
    modifier: Modifier = Modifier,
    isSelected: Boolean = false,
) {
    val isCompleted = task.status == TaskStatus.COMPLETED
    val isOverdue = task.due != null && task.due.isBefore(java.time.Instant.now()) && !isCompleted

    // Build a rich accessibility description for TalkBack.
    val statusDesc = when (task.status) {
        TaskStatus.COMPLETED -> "completed"
        TaskStatus.CANCELLED -> "cancelled"
        TaskStatus.IN_PROCESS -> "in progress"
        TaskStatus.NEEDS_ACTION -> if (isOverdue) "overdue" else "active"
    }
    val priorityDesc = when {
        task.priority in 1..4 -> ", high priority"
        task.priority == 5 -> ", medium priority"
        task.priority in 6..9 -> ", low priority"
        else -> ""
    }
    val dueDesc = task.due?.let { ", due ${dueFmt.format(it)}" } ?: ""
    val tagsDesc = if (task.categories.isEmpty()) "" else
        ", tags: ${task.categories.joinToString { "#${it.trimStart('#')}" }}"
    val subtaskDesc = if (hasChildren)
        ", $subtaskCount subtask${if (subtaskCount == 1) "" else "s"}, ${if (expanded) "expanded" else "collapsed"}"
    else ""
    val rowDescription = "${task.summary}, $statusDesc$priorityDesc$dueDesc$tagsDesc$subtaskDesc"

    // Shared element: only the selected row participates in the card→detail transition.
    val sharedTransScope = LocalSharedTransitionScope.current
    val animVisScope = LocalAnimVisibilityScope.current
    val sharedMod: Modifier = if (isSelected && sharedTransScope != null && animVisScope != null) {
        with(sharedTransScope) {
            Modifier.sharedBounds(
                rememberSharedContentState(key = "task_card_${task.uid}"),
                animatedVisibilityScope = animVisScope,
            )
        }
    } else Modifier

    Surface(
        onClick = onClick,
        modifier = modifier
            .fillMaxWidth()
            .padding(start = (depth * 20).dp, end = 4.dp)
            .then(sharedMod)
            .semantics { contentDescription = rowDescription },
        color = if (isSelected) MaterialTheme.colorScheme.secondaryContainer.copy(alpha = 0.4f)
                else Color.Transparent,
    ) {
        Row(
            modifier = Modifier
                .padding(horizontal = 12.dp, vertical = 10.dp),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            // Completion checkbox — separate interactive element for TalkBack
            Checkbox(
                checked = isCompleted,
                onCheckedChange = { onToggleComplete() },
                modifier = Modifier
                    .size(20.dp)
                    .semantics {
                        contentDescription = if (isCompleted) "Mark ${task.summary} incomplete"
                        else "Mark ${task.summary} complete"
                    },
            )

            Spacer(Modifier.width(8.dp))

            // Calendar color dot
            if (colorResolver != null) {
                CalendarColorDot(
                    calendarHref = task.calendarHref,
                    resolver = colorResolver,
                    modifier = Modifier.padding(end = 6.dp),
                )
            }

            // Text content
            Column(modifier = Modifier.weight(1f)) {
                Text(
                    text = task.summary,
                    style = MaterialTheme.typography.bodyMedium,
                    fontWeight = FontWeight.Medium,
                    color = if (isCompleted) MaterialTheme.colorScheme.onSurfaceVariant
                    else MaterialTheme.colorScheme.onSurface,
                    maxLines = 2,
                )
                Row(
                    horizontalArrangement = Arrangement.spacedBy(4.dp),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    if (task.priority > 0) {
                        PriorityChip(priority = task.priority)
                    }
                    task.due?.let { due ->
                        Text(
                            text = dueFmt.format(due),
                            style = MaterialTheme.typography.labelSmall,
                            color = if (isOverdue) MaterialTheme.colorScheme.error
                            else MaterialTheme.colorScheme.onSurfaceVariant,
                        )
                    }
                }
                if (task.categories.isNotEmpty()) {
                    TagFlow(
                        tags = task.categories,
                        magicTags = magicTags,
                        modifier = Modifier.padding(top = 4.dp),
                    )
                }
            }

            // Expand/collapse subtasks button
            if (hasChildren) {
                IconButton(
                    onClick = onExpandToggle,
                    modifier = Modifier
                        .size(32.dp)
                        .semantics {
                            contentDescription = if (expanded)
                                "Collapse subtasks of ${task.summary}"
                            else
                                "Expand subtasks of ${task.summary}"
                        },
                ) {
                    Text(
                        text = if (expanded) "▲" else "▼",
                        style = MaterialTheme.typography.labelSmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                }
            }
        }
    }

    HorizontalDivider(
        modifier = Modifier.padding(start = (depth * 20 + 16).dp),
        color = MaterialTheme.colorScheme.outlineVariant.copy(alpha = 0.4f),
        thickness = 0.5.dp,
    )
}
