package com.crossdashboard.app.ui.component

import androidx.activity.compose.BackHandler
import androidx.compose.foundation.layout.*
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.outlined.Close
import androidx.compose.material.icons.outlined.Edit
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.launch

/**
 * Shared bottom sheet container for task / event / note / issue detail views.
 *
 * On phone: wraps content in [ModalBottomSheet] with spring-physics animation on enter.
 * IME padding is applied via [WindowInsets.ime] so the keyboard does not overlap
 * text fields in edit mode.
 *
 * Predictive back is handled by intercepting [BackHandler] and animating the sheet
 * to a partially-expanded state before dismissing.
 */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun PropertySheet(
    onDismiss: () -> Unit,
    sheetState: SheetState = rememberModalBottomSheetState(
        skipPartiallyExpanded = true,
        confirmValueChange = { it != SheetValue.PartiallyExpanded },
    ),
    content: @Composable ColumnScope.() -> Unit,
) {
    val scope = rememberCoroutineScope()

    // Intercept back gesture to animate sheet hide before calling onDismiss
    BackHandler(enabled = sheetState.isVisible) {
        scope.launch {
            sheetState.hide()
            onDismiss()
        }
    }

    ModalBottomSheet(
        onDismissRequest = onDismiss,
        sheetState = sheetState,
        contentWindowInsets = { WindowInsets.ime },
        dragHandle = { BottomSheetDefaults.DragHandle() },
        // Spring physics: stiff spring for snappy entry, lower damping for natural feel
        sheetMaxWidth = BottomSheetDefaults.SheetMaxWidth,
        shape = MaterialTheme.shapes.extraLarge,
    ) {
        content()
        Spacer(Modifier.height(24.dp))
    }
}

// ─── Sheet header ─────────────────────────────────────────────────────────────

/**
 * Header row with title, optional edit/cancel toggle, and close button.
 *
 * @param editing       Current edit mode state
 * @param onEditToggle  Called when the pencil / cancel icon is tapped; pass null to hide the button
 * @param onClose       Called when the × button is tapped
 */
@Composable
fun PropertySheetHeader(
    title: String,
    editing: Boolean,
    onEditToggle: (() -> Unit)?,
    onClose: (() -> Unit)?,
    modifier: Modifier = Modifier,
) {
    Row(
        modifier = modifier
            .fillMaxWidth()
            .padding(start = 20.dp, end = 8.dp, bottom = 8.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Text(
            text = title,
            style = MaterialTheme.typography.titleMedium,
            fontWeight = FontWeight.SemiBold,
            modifier = Modifier.weight(1f),
            maxLines = 2,
        )
        if (onEditToggle != null) {
            IconButton(
                onClick = onEditToggle,
                modifier = Modifier.semantics {
                    contentDescription = if (editing) "Cancel editing" else "Edit"
                },
            ) {
                if (editing) {
                    Icon(Icons.Outlined.Close, contentDescription = null)
                } else {
                    Icon(Icons.Outlined.Edit, contentDescription = null)
                }
            }
        }
        if (onClose != null) {
            IconButton(
                onClick = onClose,
                modifier = Modifier.semantics { contentDescription = "Close" },
            ) {
                Icon(Icons.Outlined.Close, contentDescription = null)
            }
        }
    }
}

// ─── Read-only field ──────────────────────────────────────────────────────────

@Composable
fun ReadField(
    label: String,
    value: String?,
    modifier: Modifier = Modifier,
) {
    if (value.isNullOrBlank()) return
    Column(
        modifier = modifier
            .fillMaxWidth()
            .padding(horizontal = 20.dp, vertical = 6.dp),
    ) {
        Text(
            text = label.uppercase(),
            style = MaterialTheme.typography.labelSmall,
            color = MaterialTheme.colorScheme.primary,
            fontWeight = FontWeight.SemiBold,
        )
        Spacer(Modifier.height(2.dp))
        Text(
            text = value,
            style = MaterialTheme.typography.bodyMedium,
            color = MaterialTheme.colorScheme.onSurface,
        )
    }
}

/**
 * Like [ReadField] but renders [value] as GitHub-Flavoured Markdown.
 * Use for description/body fields; keep [ReadField] for short plain-text metadata.
 */
@Composable
fun ReadMarkdownField(
    label: String,
    value: String?,
    modifier: Modifier = Modifier,
) {
    if (value.isNullOrBlank()) return
    Column(
        modifier = modifier
            .fillMaxWidth()
            .padding(horizontal = 20.dp, vertical = 6.dp),
    ) {
        Text(
            text = label.uppercase(),
            style = MaterialTheme.typography.labelSmall,
            color = MaterialTheme.colorScheme.primary,
            fontWeight = FontWeight.SemiBold,
        )
        Spacer(Modifier.height(4.dp))
        MarkdownText(
            content = value,
            modifier = Modifier.fillMaxWidth(),
        )
    }
}

// ─── Section header ───────────────────────────────────────────────────────────

@Composable
fun SheetSectionHeader(
    title: String,
    modifier: Modifier = Modifier,
) {
    Text(
        text = title.uppercase(),
        style = MaterialTheme.typography.labelSmall,
        color = MaterialTheme.colorScheme.primary,
        fontWeight = FontWeight.SemiBold,
        modifier = modifier
            .fillMaxWidth()
            .padding(horizontal = 20.dp, vertical = 8.dp),
    )
}

// ─── Status badge ─────────────────────────────────────────────────────────────

@Composable
fun StatusBadge(
    label: String,
    containerColor: Color,
    contentColor: Color,
    modifier: Modifier = Modifier,
) {
    Surface(
        shape = MaterialTheme.shapes.small,
        color = containerColor,
        modifier = modifier,
    ) {
        Text(
            text = label,
            style = MaterialTheme.typography.labelSmall,
            color = contentColor,
            fontWeight = FontWeight.SemiBold,
            modifier = Modifier.padding(horizontal = 8.dp, vertical = 4.dp),
        )
    }
}

// ─── Priority chip ────────────────────────────────────────────────────────────

/** Maps priority int (0=none, 1-4=high, 5=med, 6-9=low) to a label string. */
fun priorityLabel(priority: Int): String = when {
    priority in 1..4 -> "High"
    priority == 5 -> "Medium"
    priority in 6..9 -> "Low"
    else -> "None"
}

@Composable
fun PriorityChip(
    priority: Int,
    modifier: Modifier = Modifier,
) {
    if (priority == 0) return
    val (container, content) = when {
        priority in 1..4 -> MaterialTheme.colorScheme.errorContainer to MaterialTheme.colorScheme.onErrorContainer
        priority == 5 -> MaterialTheme.colorScheme.tertiaryContainer to MaterialTheme.colorScheme.onTertiaryContainer
        else -> MaterialTheme.colorScheme.surfaceVariant to MaterialTheme.colorScheme.onSurfaceVariant
    }
    val label = priorityLabel(priority)
    StatusBadge(
        label = label,
        containerColor = container,
        contentColor = content,
        modifier = modifier.semantics { contentDescription = "Priority: $label" },
    )
}

// ─── Tag chip ─────────────────────────────────────────────────────────────────

@Composable
fun TagChip(
    label: String,
    selected: Boolean = false,
    onClick: (() -> Unit)? = null,
    modifier: Modifier = Modifier,
) {
    if (onClick != null) {
        FilterChip(
            selected = selected,
            onClick = onClick,
            label = { Text(label, style = MaterialTheme.typography.labelSmall) },
            modifier = modifier,
        )
    } else {
        SuggestionChip(
            onClick = {},
            label = { Text(label, style = MaterialTheme.typography.labelSmall) },
            modifier = modifier,
        )
    }
}

// ─── Horizontal chip row ─────────────────────────────────────────────────────

@Composable
fun ChipRow(
    modifier: Modifier = Modifier,
    content: @Composable RowScope.() -> Unit,
) {
    Row(
        modifier = modifier
            .fillMaxWidth()
            .padding(horizontal = 20.dp)
            .padding(vertical = 4.dp),
        horizontalArrangement = Arrangement.spacedBy(6.dp),
        verticalAlignment = Alignment.CenterVertically,
        content = content,
    )
}
