package com.crossdashboard.app.ui.screen.issues

import android.content.Intent
import android.net.Uri
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.outlined.Send
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.hilt.navigation.compose.hiltViewModel
import com.crossdashboard.app.domain.model.GiteaComment
import com.crossdashboard.app.domain.model.GiteaIssue
import com.crossdashboard.app.ui.component.*
import com.crossdashboard.app.ui.screen.tasks.PomodoroViewModel
import java.time.ZoneId
import java.time.format.DateTimeFormatter

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun IssuePropertySheet(
    issue: GiteaIssue,
    comments: List<GiteaComment>,
    commentLoading: Boolean,
    onDismiss: () -> Unit,
    onSave: (String, String) -> Unit,
    onToggleState: () -> Unit,
    onAddComment: (String) -> Unit,
    /** When true, renders content inline (no ModalBottomSheet wrapper) for tablet detail pane. */
    inlineMode: Boolean = false,
    pomodoroVm: PomodoroViewModel = hiltViewModel(),
) {
    var editing by remember(issue.id) { mutableStateOf(false) }
    var editTitle by remember(issue.id) { mutableStateOf(issue.title) }
    var editBody by remember(issue.id) { mutableStateOf(issue.body) }

    @Composable
    fun SheetContent() {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .then(if (!inlineMode) Modifier.navigationBarsPadding() else Modifier),
        ) {
            PropertySheetHeader(
                title = if (editing) "Edit Issue" else "#${issue.number} ${issue.title}",
                editing = editing,
                onEditToggle = {
                    if (editing) {
                        editTitle = issue.title
                        editBody = issue.body
                    }
                    editing = !editing
                },
                onClose = onDismiss,
            )

            if (editing) {
                Column(
                    modifier = Modifier
                        .fillMaxWidth()
                        .verticalScroll(rememberScrollState())
                        .padding(horizontal = 20.dp),
                    verticalArrangement = Arrangement.spacedBy(12.dp),
                ) {
                    OutlinedTextField(
                        value = editTitle,
                        onValueChange = { editTitle = it },
                        label = { Text("Title") },
                        modifier = Modifier.fillMaxWidth(),
                    )
                    OutlinedTextField(
                        value = editBody,
                        onValueChange = { editBody = it },
                        label = { Text("Body") },
                        modifier = Modifier.fillMaxWidth(),
                        minLines = 4,
                        maxLines = 10,
                    )
                    Button(
                        onClick = { onSave(editTitle.trim(), editBody.trim()) },
                        modifier = Modifier
                            .fillMaxWidth()
                            .semantics { contentDescription = "Save issue changes" },
                        enabled = editTitle.isNotBlank(),
                    ) { Text("Save") }
                    Spacer(Modifier.height(8.dp))
                }
            } else {
                IssueReadContent(
                    issue = issue,
                    comments = comments,
                    commentLoading = commentLoading,
                    onToggleState = onToggleState,
                    onAddComment = onAddComment,
                    onPomodoroStart = { pomodoroVm.start(issue.title) },
                )
            }
        }
    }

    if (inlineMode) {
        SheetContent()
    } else {
        val sheetState = rememberModalBottomSheetState(skipPartiallyExpanded = true)
        PropertySheet(onDismiss = onDismiss, sheetState = sheetState) {
            SheetContent()
        }
    }
}

// ─── Read content — also used by IssueDetailContent (tablet detail pane) ─────

@Composable
internal fun IssueReadContent(
    issue: GiteaIssue,
    comments: List<GiteaComment>,
    commentLoading: Boolean,
    onToggleState: () -> Unit,
    onAddComment: (String) -> Unit,
    onPomodoroStart: () -> Unit,
) {
    val context = LocalContext.current
    var newComment by remember { mutableStateOf("") }
    val isOpen = issue.state == "open"

    Column(modifier = Modifier.fillMaxWidth()) {
        // Scrollable section (metadata + body + comments)
        LazyColumn(
            modifier = Modifier.weight(1f, fill = false),
            contentPadding = PaddingValues(bottom = 8.dp),
        ) {
            item {
                ChipRow {
                    StatusBadge(
                        label = if (isOpen) "Open" else "Closed",
                        containerColor = if (isOpen) MaterialTheme.colorScheme.primaryContainer
                        else MaterialTheme.colorScheme.surfaceVariant,
                        contentColor = if (isOpen) MaterialTheme.colorScheme.onPrimaryContainer
                        else MaterialTheme.colorScheme.onSurfaceVariant,
                        modifier = Modifier.semantics {
                            contentDescription = "Status: ${if (isOpen) "Open" else "Closed"}"
                        },
                    )
                    Spacer(Modifier.weight(1f))
                    TextButton(
                        onClick = onPomodoroStart,
                        modifier = Modifier.semantics { contentDescription = "Start Pomodoro timer for this issue" },
                    ) {
                        Text("▶  Pomodoro", style = MaterialTheme.typography.labelMedium)
                    }
                }
            }

            item {
                ReadField(label = "Repository", value = issue.repository)
                ReadField(label = "Created", value = dtFmt.format(issue.createdAt))
                ReadField(label = "Updated", value = dtFmt.format(issue.updatedAt))
            }

            if (issue.labels.isNotEmpty()) {
                item {
                    SheetSectionHeader(title = "Labels")
                    ChipRow {
                        issue.labels.forEach { label ->
                            TagChip(label = label)
                        }
                    }
                }
            }

            if (issue.assignees.isNotEmpty()) {
                item {
                    ReadField(label = "Assignees", value = issue.assignees.joinToString(", "))
                }
            }

            if (issue.body.isNotBlank()) {
                item {
                    SheetSectionHeader(title = "Description")
                    Text(
                        text = issue.body,
                        style = MaterialTheme.typography.bodyMedium,
                        modifier = Modifier.padding(horizontal = 20.dp, vertical = 4.dp),
                    )
                }
            }

            item {
                TextButton(
                    onClick = {
                        context.startActivity(
                            Intent(Intent.ACTION_VIEW, Uri.parse(issue.htmlUrl))
                        )
                    },
                    modifier = Modifier
                        .padding(horizontal = 8.dp)
                        .semantics { contentDescription = "Open issue in browser" },
                ) {
                    Text("Open in Browser ↗", style = MaterialTheme.typography.labelMedium)
                }
            }

            item {
                Button(
                    onClick = onToggleState,
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(horizontal = 20.dp, vertical = 4.dp)
                        .semantics {
                            contentDescription = if (isOpen) "Close this issue" else "Reopen this issue"
                        },
                    colors = ButtonDefaults.buttonColors(
                        containerColor = if (isOpen) MaterialTheme.colorScheme.error
                        else MaterialTheme.colorScheme.primary,
                    ),
                ) {
                    Text(if (isOpen) "Close Issue" else "Reopen Issue")
                }
            }

            item { SheetSectionHeader(title = "Comments (${comments.size})") }

            if (commentLoading) {
                item {
                    Box(
                        modifier = Modifier
                            .fillMaxWidth()
                            .padding(16.dp),
                        contentAlignment = Alignment.Center,
                    ) {
                        CircularProgressIndicator(
                            modifier = Modifier
                                .size(20.dp)
                                .semantics { contentDescription = "Loading comments" },
                        )
                    }
                }
            } else {
                items(comments, key = { it.id }) { comment ->
                    CommentItem(comment = comment)
                }
            }
        }

        // Comment input — always visible at bottom
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = 12.dp, vertical = 8.dp),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            OutlinedTextField(
                value = newComment,
                onValueChange = { newComment = it },
                placeholder = { Text("Add comment…") },
                modifier = Modifier.weight(1f),
                maxLines = 4,
            )
            IconButton(
                onClick = {
                    if (newComment.isNotBlank()) {
                        onAddComment(newComment.trim())
                        newComment = ""
                    }
                },
                enabled = newComment.isNotBlank(),
                modifier = Modifier.semantics { contentDescription = "Post comment" },
            ) {
                Icon(Icons.Outlined.Send, contentDescription = null)
            }
        }
    }
}

// ─── Inline detail content (tablet / NavigableListDetailPaneScaffold) ─────────

/**
 * Renders the full issue detail inline, without a bottom sheet wrapper.
 * Used in the detail pane of [IssuesScreen] on tablet/Expanded windows.
 */
@Composable
fun IssueDetailContent(
    issue: GiteaIssue,
    comments: List<GiteaComment>,
    commentLoading: Boolean,
    onSave: (String, String) -> Unit,
    onToggleState: () -> Unit,
    onAddComment: (String) -> Unit,
    onDismiss: () -> Unit = {},
    pomodoroVm: PomodoroViewModel = hiltViewModel(),
) {
    IssuePropertySheet(
        issue = issue,
        comments = comments,
        commentLoading = commentLoading,
        onDismiss = onDismiss,
        onSave = onSave,
        onToggleState = onToggleState,
        onAddComment = onAddComment,
        inlineMode = true,
        pomodoroVm = pomodoroVm,
    )
}

// ─── Comment item ─────────────────────────────────────────────────────────────

private val dtFmt = DateTimeFormatter.ofPattern("d MMM yyyy, HH:mm")
    .withZone(ZoneId.systemDefault())
private val commentDtFmt = DateTimeFormatter.ofPattern("d MMM HH:mm")
    .withZone(ZoneId.systemDefault())

@Composable
private fun CommentItem(comment: GiteaComment) {
    Surface(
        shape = MaterialTheme.shapes.small,
        tonalElevation = 1.dp,
        modifier = Modifier
            .fillMaxWidth()
            .padding(horizontal = 20.dp, vertical = 4.dp)
            .semantics { contentDescription = "Comment by ${comment.user}: ${comment.body}" },
    ) {
        Column(modifier = Modifier.padding(10.dp)) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
            ) {
                Text(
                    text = comment.user,
                    style = MaterialTheme.typography.labelMedium,
                    fontWeight = FontWeight.SemiBold,
                    color = MaterialTheme.colorScheme.primary,
                )
                Text(
                    text = commentDtFmt.format(comment.createdAt),
                    style = MaterialTheme.typography.labelSmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
            Spacer(Modifier.height(4.dp))
            Text(
                text = comment.body,
                style = MaterialTheme.typography.bodySmall,
            )
        }
    }
}
