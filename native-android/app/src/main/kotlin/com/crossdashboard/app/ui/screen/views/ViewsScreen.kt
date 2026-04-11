package com.crossdashboard.app.ui.screen.views

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.KeyboardActions
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Add
import androidx.compose.material.icons.outlined.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.semantics.stateDescription
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.unit.dp
import androidx.hilt.navigation.compose.hiltViewModel
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.crossdashboard.app.ui.navigation.Destination
import java.time.ZoneId
import java.time.format.DateTimeFormatter

private val DATE_FMT = DateTimeFormatter.ofPattern("MMM d")

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun ViewsScreen(
    onNavigate: (Destination) -> Unit = {},
    vm: ViewsViewModel = hiltViewModel(),
) {
    val state by vm.state.collectAsStateWithLifecycle()

    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text("Views") },
                actions = {
                    if (state.viewMode == ViewMode.KANBAN) {
                        IconButton(
                            onClick = vm::openColumnConfig,
                            modifier = Modifier.semantics { contentDescription = "Configure Kanban columns" },
                        ) {
                            Icon(Icons.Outlined.Tune, contentDescription = null)
                        }
                    }
                },
            )
        },
        floatingActionButton = {
            FloatingActionButton(
                onClick = { /* open assign modal with no pre-selected item */ },
                modifier = Modifier.semantics { contentDescription = "Assign item to column" },
            ) {
                Icon(Icons.Default.Add, contentDescription = null)
            }
        },
    ) { padding ->
        Column(modifier = Modifier.fillMaxSize().padding(padding)) {
            // ── Mode toggle ───────────────────────────────────────────────────
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(horizontal = 12.dp, vertical = 6.dp),
                horizontalArrangement = Arrangement.spacedBy(8.dp),
            ) {
                FilterChip(
                    selected = state.viewMode == ViewMode.KANBAN,
                    onClick = { vm.setViewMode(ViewMode.KANBAN) },
                    label = { Text("Kanban") },
                    leadingIcon = {
                        Icon(Icons.Outlined.ViewColumn, contentDescription = null, modifier = Modifier.size(16.dp))
                    },
                    modifier = Modifier.semantics {
                        contentDescription = "Kanban board view"
                        stateDescription = if (state.viewMode == ViewMode.KANBAN) "selected" else "not selected"
                    },
                )
                FilterChip(
                    selected = state.viewMode == ViewMode.COVEY,
                    onClick = { vm.setViewMode(ViewMode.COVEY) },
                    label = { Text("Quadrants") },
                    leadingIcon = {
                        Icon(Icons.Outlined.GridView, contentDescription = null, modifier = Modifier.size(16.dp))
                    },
                    modifier = Modifier.semantics {
                        contentDescription = "Covey Four Quadrants view"
                        stateDescription = if (state.viewMode == ViewMode.COVEY) "selected" else "not selected"
                    },
                )
            }

            // ── Board content ─────────────────────────────────────────────────
            when (state.viewMode) {
                ViewMode.KANBAN -> KanbanBoard(state = state, onAssign = vm::openAssign, onRemove = vm::removeTag)
                ViewMode.COVEY -> CoveyBoard(state = state, onAssign = vm::openAssign, onRemove = vm::removeTag)
            }
        }
    }

    // ── Assign modal ──────────────────────────────────────────────────────────
    state.assigningItem?.let { item ->
        AssignModal(
            item = item,
            columns = when (state.viewMode) {
                ViewMode.KANBAN -> state.kanbanColumns
                ViewMode.COVEY -> CoveyTag.ALL
            },
            onAssign = { tag -> vm.assignTag(item, tag, when (state.viewMode) {
                ViewMode.KANBAN -> state.kanbanColumns
                ViewMode.COVEY -> CoveyTag.ALL
            }) },
            onDismiss = vm::closeAssign,
        )
    }

    // ── Column config modal ───────────────────────────────────────────────────
    state.editingColumns?.let { cols ->
        ColumnConfigModal(
            columns = cols,
            onSave = vm::saveColumns,
            onDismiss = vm::closeColumnConfig,
        )
    }

    // ── Error snackbar ────────────────────────────────────────────────────────
    state.error?.let { err ->
        AlertDialog(
            onDismissRequest = vm::dismissError,
            title = { Text("Error") },
            text = { Text(err) },
            confirmButton = { TextButton(onClick = vm::dismissError) { Text("OK") } },
        )
    }
}

// ─── Kanban board ──────────────────────────────────────────────────────────────

@Composable
private fun KanbanBoard(
    state: ViewsUiState,
    onAssign: (ViewItem) -> Unit,
    onRemove: (ViewItem, String) -> Unit,
) {
    Row(
        modifier = Modifier
            .fillMaxSize()
            .horizontalScroll(rememberScrollState())
            .padding(horizontal = 8.dp)
            .semantics { contentDescription = "Kanban board with ${state.kanbanColumns.size} columns" },
        horizontalArrangement = Arrangement.spacedBy(8.dp),
    ) {
        // "Untagged" column — items that don't match any column tag
        val allColumnTags = state.kanbanColumns
        val untagged = state.items.filter { item ->
            item.labels.none { lbl -> lbl in allColumnTags }
        }
        if (untagged.isNotEmpty()) {
            KanbanColumn(
                title = "Untagged",
                items = untagged,
                columnTag = null,
                onAssign = onAssign,
                onRemove = onRemove,
            )
        }

        // One column per configured tag
        state.kanbanColumns.forEach { col ->
            val colItems = state.items.filter { col in it.labels }
            KanbanColumn(
                title = col.replaceFirstChar { it.uppercaseChar() },
                items = colItems,
                columnTag = col,
                onAssign = onAssign,
                onRemove = onRemove,
            )
        }
    }
}

@Composable
private fun KanbanColumn(
    title: String,
    items: List<ViewItem>,
    columnTag: String?,
    onAssign: (ViewItem) -> Unit,
    onRemove: (ViewItem, String) -> Unit,
) {
    val headerColor = MaterialTheme.colorScheme.surfaceVariant
    val itemWord = if (items.size == 1) "item" else "items"

    Column(
        modifier = Modifier
            .width(220.dp)
            .fillMaxHeight()
            .clip(RoundedCornerShape(12.dp))
            .background(MaterialTheme.colorScheme.surface)
            .border(1.dp, MaterialTheme.colorScheme.outlineVariant, RoundedCornerShape(12.dp))
            .semantics { contentDescription = "$title column, ${items.size} $itemWord" },
    ) {
        // Column header
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .background(headerColor, RoundedCornerShape(topStart = 12.dp, topEnd = 12.dp))
                .padding(horizontal = 12.dp, vertical = 10.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.SpaceBetween,
        ) {
            Text(
                title,
                style = MaterialTheme.typography.titleSmall,
                fontWeight = FontWeight.SemiBold,
            )
            Text(
                "${items.size}",
                style = MaterialTheme.typography.labelMedium,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
        }

        LazyColumn(
            modifier = Modifier.fillMaxWidth(),
            contentPadding = PaddingValues(8.dp),
            verticalArrangement = Arrangement.spacedBy(6.dp),
        ) {
            items(items, key = { it.key }) { item ->
                ViewItemCard(
                    item = item,
                    onAssign = onAssign,
                    onRemove = { columnTag?.let { tag -> onRemove(item, tag) } },
                )
            }
        }
    }
}

// ─── Covey quadrants ──────────────────────────────────────────────────────────

private data class CoveyQuadrant(
    val tag: String,
    val label: String,
    val description: String,
)

private val COVEY_QUADRANTS = listOf(
    CoveyQuadrant(CoveyTag.DO, "Do", "Urgent + Important"),
    CoveyQuadrant(CoveyTag.DELAY, "Delay", "Not Urgent + Important"),
    CoveyQuadrant(CoveyTag.DELEGATE, "Delegate", "Urgent + Not Important"),
    CoveyQuadrant(CoveyTag.ELIMINATE, "Eliminate", "Not Urgent + Not Important"),
)

@Composable
private fun CoveyBoard(
    state: ViewsUiState,
    onAssign: (ViewItem) -> Unit,
    onRemove: (ViewItem, String) -> Unit,
) {
    Column(
        modifier = Modifier
            .fillMaxSize()
            .padding(8.dp),
        verticalArrangement = Arrangement.spacedBy(8.dp),
    ) {
        Row(
            modifier = Modifier.fillMaxWidth().weight(1f),
            horizontalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            CoveyQuadrantCell(
                quadrant = COVEY_QUADRANTS[0],
                items = state.items.filter { CoveyTag.DO in it.labels },
                onAssign = onAssign,
                onRemove = { item -> onRemove(item, CoveyTag.DO) },
                modifier = Modifier.weight(1f).fillMaxHeight(),
                containerColor = MaterialTheme.colorScheme.errorContainer.copy(alpha = 0.25f),
            )
            CoveyQuadrantCell(
                quadrant = COVEY_QUADRANTS[1],
                items = state.items.filter { CoveyTag.DELAY in it.labels },
                onAssign = onAssign,
                onRemove = { item -> onRemove(item, CoveyTag.DELAY) },
                modifier = Modifier.weight(1f).fillMaxHeight(),
                containerColor = MaterialTheme.colorScheme.primaryContainer.copy(alpha = 0.25f),
            )
        }
        Row(
            modifier = Modifier.fillMaxWidth().weight(1f),
            horizontalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            CoveyQuadrantCell(
                quadrant = COVEY_QUADRANTS[2],
                items = state.items.filter { CoveyTag.DELEGATE in it.labels },
                onAssign = onAssign,
                onRemove = { item -> onRemove(item, CoveyTag.DELEGATE) },
                modifier = Modifier.weight(1f).fillMaxHeight(),
                containerColor = MaterialTheme.colorScheme.secondaryContainer.copy(alpha = 0.25f),
            )
            CoveyQuadrantCell(
                quadrant = COVEY_QUADRANTS[3],
                items = state.items.filter { CoveyTag.ELIMINATE in it.labels },
                onAssign = onAssign,
                onRemove = { item -> onRemove(item, CoveyTag.ELIMINATE) },
                modifier = Modifier.weight(1f).fillMaxHeight(),
                containerColor = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.5f),
            )
        }
    }
}

@Composable
private fun CoveyQuadrantCell(
    quadrant: CoveyQuadrant,
    items: List<ViewItem>,
    onAssign: (ViewItem) -> Unit,
    onRemove: (ViewItem) -> Unit,
    modifier: Modifier = Modifier,
    containerColor: androidx.compose.ui.graphics.Color,
) {
    val itemWord = if (items.size == 1) "item" else "items"
    Column(
        modifier = modifier
            .clip(RoundedCornerShape(12.dp))
            .background(containerColor)
            .border(1.dp, MaterialTheme.colorScheme.outlineVariant, RoundedCornerShape(12.dp))
            .padding(8.dp)
            .semantics {
                contentDescription = "${quadrant.label} quadrant: ${quadrant.description}, ${items.size} $itemWord"
            },
    ) {
        Text(quadrant.label, style = MaterialTheme.typography.titleSmall, fontWeight = FontWeight.SemiBold)
        Text(quadrant.description, style = MaterialTheme.typography.labelSmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
        Spacer(Modifier.height(4.dp))
        LazyColumn(
            modifier = Modifier.fillMaxWidth(),
            verticalArrangement = Arrangement.spacedBy(4.dp),
        ) {
            items(items, key = { it.key }) { item ->
                ViewItemCard(item = item, onAssign = onAssign, onRemove = { onRemove(item) })
            }
        }
    }
}

// ─── Item card ────────────────────────────────────────────────────────────────

@Composable
private fun ViewItemCard(
    item: ViewItem,
    onAssign: (ViewItem) -> Unit,
    onRemove: () -> Unit,
) {
    val zone = ZoneId.systemDefault()
    val typeDesc = if (item.isTask) "Task" else "Issue"
    val priorityDesc = when {
        item.priority in 1..4 -> ", high priority"
        item.priority == 5 -> ", medium priority"
        item.priority in 6..9 -> ", low priority"
        else -> ""
    }
    val dueDesc = item.due?.let { ", due ${it.atZone(zone).format(DATE_FMT)}" } ?: ""
    val cardDesc = "$typeDesc: ${item.title}$priorityDesc$dueDesc. Tap to reassign"

    Surface(
        modifier = Modifier
            .fillMaxWidth()
            .clickable { onAssign(item) }
            .semantics { contentDescription = cardDesc },
        shape = RoundedCornerShape(8.dp),
        tonalElevation = 1.dp,
    ) {
        Row(
            modifier = Modifier.padding(horizontal = 10.dp, vertical = 8.dp),
            verticalAlignment = Alignment.Top,
            horizontalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            // Type icon — decorative, full description already on container
            Icon(
                if (item.isTask) Icons.Outlined.CheckBoxOutlineBlank else Icons.Outlined.BugReport,
                contentDescription = null,
                modifier = Modifier.size(14.dp).padding(top = 2.dp),
                tint = MaterialTheme.colorScheme.onSurfaceVariant,
            )
            Column(modifier = Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(2.dp)) {
                Text(item.title, style = MaterialTheme.typography.bodySmall, maxLines = 2)
                if (item.priority in 1..4) {
                    Text("⚑ High", style = MaterialTheme.typography.labelSmall, color = MaterialTheme.colorScheme.error)
                }
                item.due?.let { due ->
                    Text(
                        due.atZone(zone).format(DATE_FMT),
                        style = MaterialTheme.typography.labelSmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                }
            }
        }
    }
}

// ─── Assign modal ──────────────────────────────────────────────────────────────

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun AssignModal(
    item: ViewItem,
    columns: List<String>,
    onAssign: (String) -> Unit,
    onDismiss: () -> Unit,
) {
    ModalBottomSheet(
        onDismissRequest = onDismiss,
        modifier = Modifier.semantics { contentDescription = "Assign ${item.title} to a column" },
    ) {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = 16.dp)
                .padding(bottom = 24.dp),
            verticalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            Text("Assign to", style = MaterialTheme.typography.titleMedium)
            Text(
                item.title,
                style = MaterialTheme.typography.bodyMedium,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
                maxLines = 2,
            )
            Spacer(Modifier.height(4.dp))
            columns.forEach { col ->
                val isSelected = col in item.labels
                val displayName = col.replaceFirstChar { it.uppercaseChar() }
                Row(
                    modifier = Modifier
                        .fillMaxWidth()
                        .clip(RoundedCornerShape(8.dp))
                        .clickable { onAssign(col) }
                        .background(
                            if (isSelected) MaterialTheme.colorScheme.primaryContainer
                            else MaterialTheme.colorScheme.surface,
                        )
                        .padding(horizontal = 16.dp, vertical = 12.dp)
                        .semantics(mergeDescendants = true) {
                            contentDescription = displayName
                            stateDescription = if (isSelected) "currently assigned" else "not assigned"
                        },
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.SpaceBetween,
                ) {
                    Text(
                        displayName,
                        style = MaterialTheme.typography.bodyMedium,
                        fontWeight = if (isSelected) FontWeight.SemiBold else FontWeight.Normal,
                    )
                    if (isSelected) {
                        Icon(Icons.Outlined.Check, contentDescription = null, tint = MaterialTheme.colorScheme.primary)
                    }
                }
            }
        }
    }
}

// ─── Column config modal ───────────────────────────────────────────────────────

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun ColumnConfigModal(
    columns: List<String>,
    onSave: (List<String>) -> Unit,
    onDismiss: () -> Unit,
) {
    var editCols by remember(columns) { mutableStateOf(columns.toMutableList()) }
    var newColText by remember { mutableStateOf("") }

    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text("Kanban Columns") },
        text = {
            Column(verticalArrangement = Arrangement.spacedBy(6.dp)) {
                editCols.forEachIndexed { _, col ->
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        Text(
                            col,
                            modifier = Modifier.weight(1f),
                            style = MaterialTheme.typography.bodyMedium,
                        )
                        IconButton(
                            onClick = { editCols = (editCols - col).toMutableList() },
                            modifier = Modifier
                                .size(32.dp)
                                .semantics { contentDescription = "Remove column $col" },
                        ) {
                            Icon(Icons.Outlined.Close, contentDescription = null, modifier = Modifier.size(16.dp))
                        }
                    }
                }
                HorizontalDivider()
                OutlinedTextField(
                    value = newColText,
                    onValueChange = { newColText = it },
                    label = { Text("New column name") },
                    singleLine = true,
                    keyboardOptions = KeyboardOptions(imeAction = ImeAction.Done),
                    keyboardActions = KeyboardActions(onDone = {
                        if (newColText.isNotBlank()) {
                            editCols = (editCols + newColText.trim()).toMutableList()
                            newColText = ""
                        }
                    }),
                    trailingIcon = {
                        IconButton(
                            onClick = {
                                if (newColText.isNotBlank()) {
                                    editCols = (editCols + newColText.trim()).toMutableList()
                                    newColText = ""
                                }
                            },
                            modifier = Modifier.semantics { contentDescription = "Add column" },
                        ) {
                            Icon(Icons.Default.Add, contentDescription = null)
                        }
                    },
                )
            }
        },
        confirmButton = {
            TextButton(onClick = { onSave(editCols) }) { Text("Save") }
        },
        dismissButton = {
            TextButton(onClick = onDismiss) { Text("Cancel") }
        },
    )
}
