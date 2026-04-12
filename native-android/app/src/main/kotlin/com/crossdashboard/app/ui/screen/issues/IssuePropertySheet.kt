package com.crossdashboard.app.ui.screen.issues

import android.content.Intent
import android.net.Uri
import android.provider.OpenableColumns
import androidx.compose.foundation.clickable
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.outlined.Send
import androidx.compose.material.icons.outlined.AttachFile
import androidx.compose.material.icons.outlined.Close
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
import com.crossdashboard.app.domain.model.GiteaAttachment
import com.crossdashboard.app.domain.model.GiteaComment
import com.crossdashboard.app.domain.model.GiteaIssue
import com.crossdashboard.app.ui.component.*
import com.crossdashboard.app.ui.screen.tasks.PomodoroViewModel
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.time.ZoneId
import java.time.format.DateTimeFormatter

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun IssuePropertySheet(
    issue: GiteaIssue,
    comments: List<GiteaComment>,
    commentLoading: Boolean,
    issueAttachments: List<GiteaAttachment> = emptyList(),
    commentAttachments: Map<Long, List<GiteaAttachment>> = emptyMap(),
    onDismiss: () -> Unit,
    onSave: (String, String) -> Unit,
    onToggleState: () -> Unit,
    onAddComment: (body: String, attachments: List<PendingAttachment>) -> Unit,
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
                    issueAttachments = issueAttachments,
                    commentAttachments = commentAttachments,
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
    issueAttachments: List<GiteaAttachment> = emptyList(),
    commentAttachments: Map<Long, List<GiteaAttachment>> = emptyMap(),
    onToggleState: () -> Unit,
    onAddComment: (body: String, attachments: List<PendingAttachment>) -> Unit,
    onPomodoroStart: () -> Unit,
) {
    val context = LocalContext.current
    val scope = rememberCoroutineScope()
    var newComment by remember { mutableStateOf("") }
    var pendingAttachments by remember { mutableStateOf<List<PendingAttachment>>(emptyList()) }
    val isOpen = issue.state == "open"

    val fileLauncher = rememberLauncherForActivityResult(
        ActivityResultContracts.GetContent()
    ) { uri ->
        uri ?: return@rememberLauncherForActivityResult
        scope.launch {
            val bytes = withContext(Dispatchers.IO) {
                context.contentResolver.openInputStream(uri)?.use { it.readBytes() }
            } ?: return@launch
            val mimeType = context.contentResolver.getType(uri) ?: "application/octet-stream"
            val fileName = run {
                var name = "attachment"
                context.contentResolver.query(uri, null, null, null, null)?.use { cursor ->
                    if (cursor.moveToFirst()) {
                        val idx = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME)
                        if (idx >= 0) name = cursor.getString(idx)
                    }
                }
                name
            }
            pendingAttachments = pendingAttachments + PendingAttachment(fileName, mimeType, bytes)
        }
    }

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
                    MarkdownText(
                        content = issue.body,
                        modifier = Modifier.padding(horizontal = 20.dp, vertical = 4.dp),
                    )
                }
            }

            if (issueAttachments.isNotEmpty()) {
                item {
                    SheetSectionHeader(title = "Attachments (${issueAttachments.size})")
                    Column(
                        modifier = Modifier.padding(horizontal = 20.dp, vertical = 4.dp),
                        verticalArrangement = Arrangement.spacedBy(6.dp),
                    ) {
                        issueAttachments.forEach { att ->
                            AttachmentLink(attachment = att)
                        }
                    }
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
                    CommentItem(
                        comment = comment,
                        attachments = commentAttachments[comment.id] ?: emptyList(),
                    )
                }
            }
        }

        // Pending comment attachments
        if (pendingAttachments.isNotEmpty()) {
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(horizontal = 12.dp),
                horizontalArrangement = Arrangement.spacedBy(4.dp),
            ) {
                pendingAttachments.forEach { att ->
                    InputChip(
                        selected = false,
                        onClick = {},
                        label = { Text(att.fileName, style = MaterialTheme.typography.labelSmall) },
                        trailingIcon = {
                            IconButton(
                                onClick = { pendingAttachments = pendingAttachments - att },
                                modifier = Modifier.size(18.dp),
                            ) {
                                Icon(
                                    Icons.Outlined.Close,
                                    contentDescription = "Remove ${att.fileName}",
                                    modifier = Modifier.size(14.dp),
                                )
                            }
                        },
                    )
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
                onClick = { fileLauncher.launch("*/*") },
                modifier = Modifier.semantics { contentDescription = "Attach file to comment" },
            ) {
                Icon(Icons.Outlined.AttachFile, contentDescription = null)
            }
            IconButton(
                onClick = {
                    if (newComment.isNotBlank() || pendingAttachments.isNotEmpty()) {
                        onAddComment(newComment.trim(), pendingAttachments)
                        newComment = ""
                        pendingAttachments = emptyList()
                    }
                },
                enabled = newComment.isNotBlank() || pendingAttachments.isNotEmpty(),
                modifier = Modifier.semantics { contentDescription = "Post comment" },
            ) {
                Icon(Icons.AutoMirrored.Outlined.Send, contentDescription = null)
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
    issueAttachments: List<GiteaAttachment> = emptyList(),
    commentAttachments: Map<Long, List<GiteaAttachment>> = emptyMap(),
    onSave: (String, String) -> Unit,
    onToggleState: () -> Unit,
    onAddComment: (body: String, attachments: List<PendingAttachment>) -> Unit,
    onDismiss: () -> Unit = {},
    pomodoroVm: PomodoroViewModel = hiltViewModel(),
) {
    IssuePropertySheet(
        issue = issue,
        comments = comments,
        commentLoading = commentLoading,
        issueAttachments = issueAttachments,
        commentAttachments = commentAttachments,
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
private fun CommentItem(
    comment: GiteaComment,
    attachments: List<GiteaAttachment> = emptyList(),
) {
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
            MarkdownText(
                content = comment.body,
                modifier = Modifier.fillMaxWidth(),
            )
            if (attachments.isNotEmpty()) {
                Spacer(Modifier.height(6.dp))
                HorizontalDivider(color = MaterialTheme.colorScheme.outlineVariant.copy(alpha = 0.4f))
                Spacer(Modifier.height(6.dp))
                Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
                    attachments.forEach { att ->
                        AttachmentLink(attachment = att)
                    }
                }
            }
        }
    }
}

// ─── Attachment hyperlink row ─────────────────────────────────────────────────

@Composable
private fun AttachmentLink(attachment: GiteaAttachment) {
    val context = LocalContext.current
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .clickable {
                context.startActivity(
                    Intent(Intent.ACTION_VIEW, Uri.parse(attachment.downloadUrl))
                )
            }
            .semantics { contentDescription = "Open attachment ${attachment.name}" }
            .padding(vertical = 2.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(6.dp),
    ) {
        Icon(
            Icons.Outlined.AttachFile,
            contentDescription = null,
            modifier = Modifier.size(14.dp),
            tint = MaterialTheme.colorScheme.primary,
        )
        Text(
            text = attachment.name,
            style = MaterialTheme.typography.labelMedium,
            color = MaterialTheme.colorScheme.primary,
        )
        if (attachment.size > 0) {
            Text(
                text = formatFileSize(attachment.size),
                style = MaterialTheme.typography.labelSmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
        }
    }
}

private fun formatFileSize(bytes: Long): String = when {
    bytes < 1024 -> "${bytes} B"
    bytes < 1024 * 1024 -> "${"%.1f".format(bytes / 1024.0)} KB"
    else -> "${"%.1f".format(bytes / (1024.0 * 1024))} MB"
}
