package com.crossdashboard.app.ui.component

import androidx.compose.foundation.clickable
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.foundation.layout.*
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.outlined.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.unit.dp
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.window.core.layout.WindowSizeClass
import androidx.compose.material3.adaptive.currentWindowAdaptiveInfo

data class FilterChoice(val key: String, val label: String)

data class AdaptiveFilterSpec(
    val title: String,
    val choices: List<FilterChoice>,
    val selectedKeys: Set<String>,
    val multiSelect: Boolean = false,
    val searchable: Boolean = false,
    val defaultKeys: Set<String>? = null,
    val onSelectionChange: (Set<String>) -> Unit,
)

/** Inline searchable menus on larger windows; a single filter sheet on compact windows. */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun AdaptiveFilterBar(
    filters: List<AdaptiveFilterSpec>,
    hasActiveFilters: Boolean,
    onClear: () -> Unit,
    modifier: Modifier = Modifier,
) {
    val expanded = currentWindowAdaptiveInfo().windowSizeClass
        .isWidthAtLeastBreakpoint(WindowSizeClass.WIDTH_DP_MEDIUM_LOWER_BOUND)
    var showSheet by remember { mutableStateOf(false) }

    Row(
        modifier = modifier
            .fillMaxWidth()
            .padding(horizontal = 12.dp, vertical = 6.dp),
        horizontalArrangement = Arrangement.spacedBy(8.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        if (expanded) {
            filters.forEach { spec -> FilterDropdown(spec) }
        } else {
            OutlinedButton(onClick = { showSheet = true }) {
                Icon(Icons.Outlined.FilterList, contentDescription = null)
                Spacer(Modifier.width(6.dp))
                Text(if (hasActiveFilters) "Filters applied" else "Filters")
            }
        }

        if (hasActiveFilters) {
            IconButton(
                onClick = onClear,
                modifier = Modifier.semantics { contentDescription = "Clear filters" },
            ) {
                Icon(Icons.Outlined.Close, contentDescription = null)
            }
        }
    }

    if (showSheet) {
        ModalBottomSheet(onDismissRequest = { showSheet = false }) {
            Column(
                modifier = Modifier
                    .fillMaxWidth()
                    .heightIn(max = 680.dp)
                    .verticalScroll(rememberScrollState()),
            ) {
                Text(
                    "Filters",
                    style = MaterialTheme.typography.titleLarge,
                    modifier = Modifier.padding(horizontal = 20.dp),
                )
                filters.forEach { spec ->
                    FilterSheetSection(spec)
                    HorizontalDivider()
                }
                Spacer(Modifier.height(24.dp))
            }
        }
    }
}

@Composable
private fun FilterDropdown(spec: AdaptiveFilterSpec) {
    var expanded by remember { mutableStateOf(false) }
    var query by remember { mutableStateOf("") }
    val selectedLabels = spec.choices.filter { it.key in spec.selectedKeys }.map { it.label }
    val buttonLabel = when {
        selectedLabels.isEmpty() -> spec.title
        selectedLabels.size == 1 -> "${spec.title} – ${selectedLabels.first()}"
        else -> "${spec.title} – ${selectedLabels.size} selected"
    }
    val defaultKeys = spec.defaultKeys ?: if (spec.multiSelect) emptySet()
        else spec.choices.firstOrNull()?.let { setOf(it.key) }.orEmpty()
    val isActive = spec.selectedKeys != defaultKeys

    Box {
        OutlinedButton(
            onClick = { expanded = true },
            colors = ButtonDefaults.outlinedButtonColors(
                containerColor = if (isActive) MaterialTheme.colorScheme.secondaryContainer
                    else MaterialTheme.colorScheme.surface,
                contentColor = if (isActive) MaterialTheme.colorScheme.onSecondaryContainer
                    else MaterialTheme.colorScheme.onSurface,
            ),
            border = BorderStroke(
                1.dp,
                if (isActive) MaterialTheme.colorScheme.secondary else MaterialTheme.colorScheme.outline,
            ),
        ) {
            Icon(filterIcon(spec.title), contentDescription = null, modifier = Modifier.size(18.dp))
            Spacer(Modifier.width(6.dp))
            Text(buttonLabel)
        }
        DropdownMenu(
            expanded = expanded,
            onDismissRequest = {
                expanded = false
                query = ""
            },
            modifier = Modifier.widthIn(min = 240.dp, max = 320.dp),
        ) {
            if (spec.searchable) {
                OutlinedTextField(
                    value = query,
                    onValueChange = { query = it },
                    label = { Text("Search ${spec.title.lowercase()}") },
                    singleLine = true,
                    modifier = Modifier.padding(8.dp).fillMaxWidth(),
                )
                HorizontalDivider()
            }
            filteredChoices(spec, query).forEach { choice ->
                DropdownMenuItem(
                    text = { Text(choice.label) },
                    leadingIcon = {
                        Row(verticalAlignment = Alignment.CenterVertically) {
                            if (spec.multiSelect) {
                                Checkbox(checked = choice.key in spec.selectedKeys, onCheckedChange = null)
                            } else {
                                RadioButton(selected = choice.key in spec.selectedKeys, onClick = null)
                            }
                            choiceIcon(spec.title, choice.key)?.let {
                                Icon(it, contentDescription = null, modifier = Modifier.size(18.dp))
                            }
                        }
                    },
                    onClick = {
                        updateSelection(spec, choice.key)
                        if (!spec.multiSelect) expanded = false
                    },
                )
            }
        }
    }
}

@Composable
private fun FilterSheetSection(spec: AdaptiveFilterSpec) {
    var query by remember(spec.title) { mutableStateOf("") }
    Column(modifier = Modifier.padding(horizontal = 20.dp, vertical = 12.dp)) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Icon(filterIcon(spec.title), contentDescription = null, modifier = Modifier.size(18.dp))
            Spacer(Modifier.width(8.dp))
            Text(spec.title, style = MaterialTheme.typography.titleSmall)
        }
        if (spec.searchable) {
            OutlinedTextField(
                value = query,
                onValueChange = { query = it },
                label = { Text("Search ${spec.title.lowercase()}") },
                singleLine = true,
                modifier = Modifier.fillMaxWidth().padding(top = 8.dp),
            )
        }
        filteredChoices(spec, query).forEach { choice ->
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .clickable { updateSelection(spec, choice.key) }
                    .padding(vertical = 8.dp),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                if (spec.multiSelect) {
                    Checkbox(checked = choice.key in spec.selectedKeys, onCheckedChange = null)
                } else {
                    RadioButton(selected = choice.key in spec.selectedKeys, onClick = null)
                }
                choiceIcon(spec.title, choice.key)?.let {
                    Spacer(Modifier.width(4.dp))
                    Icon(it, contentDescription = null, modifier = Modifier.size(18.dp))
                }
                Spacer(Modifier.width(8.dp))
                Text(choice.label)
            }
        }
    }
}

private fun filterIcon(title: String): ImageVector = when (title) {
    "Tags" -> Icons.Outlined.LocalOffer
    "Time range" -> Icons.Outlined.DateRange
    "Type" -> Icons.Outlined.Category
    "Status" -> Icons.Outlined.CheckCircle
    "Milestone" -> Icons.Outlined.Flag
    else -> Icons.Outlined.FilterList
}

private fun choiceIcon(title: String, key: String): ImageVector? {
    val value = key.lowercase()
    return when (title) {
        "Status" -> when (value) {
            "open" -> Icons.Outlined.RadioButtonUnchecked
            "closed", "completed" -> Icons.Outlined.CheckCircle
            "archived" -> Icons.Outlined.Archive
            "active", "normal" -> Icons.Outlined.Bolt
            "all" -> Icons.Outlined.List
            else -> null
        }
        "Type" -> when (value) {
            "events" -> Icons.Outlined.CalendarMonth
            "tasks" -> Icons.Outlined.Checklist
            "issues" -> Icons.Outlined.ErrorOutline
            "all" -> Icons.Outlined.Category
            else -> null
        }
        "Time range" -> when (value) {
            "today" -> Icons.Outlined.Today
            "tomorrow" -> Icons.Outlined.Event
            "this_week", "week" -> Icons.Outlined.DateRange
            "all" -> Icons.Outlined.AllInclusive
            else -> null
        }
        "Milestone" -> Icons.Outlined.Flag
        else -> null
    }
}

private fun filteredChoices(spec: AdaptiveFilterSpec, query: String): List<FilterChoice> =
    if (query.isBlank()) spec.choices else spec.choices.filter {
        it.label.contains(query.trim(), ignoreCase = true)
    }

private fun updateSelection(spec: AdaptiveFilterSpec, key: String) {
    val updated = if (spec.multiSelect) {
        if (key in spec.selectedKeys) spec.selectedKeys - key else spec.selectedKeys + key
    } else {
        setOf(key)
    }
    spec.onSelectionChange(updated)
}
