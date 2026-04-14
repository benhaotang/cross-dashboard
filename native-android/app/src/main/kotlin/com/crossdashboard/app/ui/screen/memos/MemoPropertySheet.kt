package com.crossdashboard.app.ui.screen.memos

import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.content.Intent
import android.graphics.BitmapFactory
import android.net.Uri
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.outlined.*
import androidx.compose.material3.*
import androidx.compose.material3.adaptive.currentWindowAdaptiveInfo
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.foundation.Image
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.unit.dp
import androidx.window.core.layout.WindowSizeClass
import com.crossdashboard.app.domain.model.*
import com.crossdashboard.app.ui.component.MarkdownText
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import java.net.HttpURLConnection
import java.net.URL
import java.time.ZoneId
import java.time.format.DateTimeFormatter

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun MemoPropertySheet(
    memo: MemosMemo,
    comments: List<MemosMemo>,
    commentsLoading: Boolean,
    memosHost: String,
    memosToken: String,
    issuesByRepo: Map<String, List<GiteaIssue>>,
    onLoadComments: () -> Unit,
    onAddComment: (String) -> Unit,
    onShare: () -> Unit,
    onDelete: () -> Unit,
    onArchive: () -> Unit,
    onExtractTasks: (MemosMemo) -> List<ParsedTask>,
    onDetectDate: (MemosMemo) -> java.time.Instant?,
    onFirstUrl: (MemosMemo) -> String?,
    vm: MemosViewModel,
    modifier: Modifier = Modifier,
) {
    val windowInfo = currentWindowAdaptiveInfo()
    val isExpanded = windowInfo.windowSizeClass.isWidthAtLeastBreakpoint(WindowSizeClass.WIDTH_DP_MEDIUM_LOWER_BOUND)

    LaunchedEffect(memo.name) { onLoadComments() }

    val context = LocalContext.current
    var commentInput by remember { mutableStateOf("") }
    var showExtractSheet by remember { mutableStateOf(false) }
    var showEventSheet by remember { mutableStateOf(false) }
    var showCommentIssueSheet by remember { mutableStateOf(false) }

    val dateFormatter = DateTimeFormatter.ofPattern("MMM d, yyyy HH:mm").withZone(ZoneId.systemDefault())
    val memoId = memo.name   // "memos/{id}"
    val memoUrl = if (memosHost.isNotBlank()) "$memosHost/$memoId" else null

    // Determine which action buttons are relevant
    val hasIncompleteTasks = memo.property.hasIncompleteTasks
    val hasDate = onDetectDate(memo) != null
    val hasGitea = issuesByRepo.isNotEmpty()
    val hasLink = memo.property.hasLink
    val firstUrl = onFirstUrl(memo)

    LazyColumn(
        modifier = modifier.fillMaxSize(),
        contentPadding = PaddingValues(bottom = 80.dp),
    ) {
        // ── Header ───────────────────────────────────────────────────────────
        item {
            Column(modifier = Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(4.dp)) {
                if (memo.property.title.isNotBlank()) {
                    Text(memo.property.title, style = MaterialTheme.typography.titleLarge)
                }
                Text(
                    "${dateFormatter.format(memo.displayTime)} · ${memo.visibility.name.lowercase()}",
                    style = MaterialTheme.typography.labelSmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
                if (memo.tags.isNotEmpty()) {
                    Row(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                        memo.tags.forEach { tag ->
                            SuggestionChip(onClick = {}, label = { Text("#$tag", style = MaterialTheme.typography.labelSmall) })
                        }
                    }
                }
            }
        }

        // ── Markdown body ─────────────────────────────────────────────────────
        item {
            MarkdownText(
                content = memo.content,
                modifier = Modifier.padding(horizontal = 16.dp, vertical = 4.dp),
            )
        }

        // ── Action toolbar ────────────────────────────────────────────────────
        item {
            HorizontalDivider(modifier = Modifier.padding(vertical = 8.dp))
            if (isExpanded) {
                // Tablet / expanded: labeled buttons
                Row(
                    modifier = Modifier.padding(horizontal = 16.dp, vertical = 4.dp),
                    horizontalArrangement = Arrangement.spacedBy(8.dp),
                ) {
                    if (hasIncompleteTasks) {
                        FilledTonalButton(onClick = { showExtractSheet = true }) {
                            Icon(Icons.Outlined.ChecklistRtl, contentDescription = null, modifier = Modifier.size(18.dp))
                            Spacer(Modifier.width(4.dp))
                            Text("Extract Tasks")
                        }
                    }
                    if (hasDate) {
                        FilledTonalButton(onClick = { showEventSheet = true }) {
                            Icon(Icons.Outlined.CalendarMonth, contentDescription = null, modifier = Modifier.size(18.dp))
                            Spacer(Modifier.width(4.dp))
                            Text("Create Event")
                        }
                    }
                    if (hasGitea) {
                        FilledTonalButton(onClick = { showCommentIssueSheet = true }) {
                            Icon(Icons.Outlined.Comment, contentDescription = null, modifier = Modifier.size(18.dp))
                            Spacer(Modifier.width(4.dp))
                            Text("Comment Issue")
                        }
                    }
                    if (hasLink && firstUrl != null) {
                        FilledTonalButton(onClick = {
                            context.startActivity(Intent(Intent.ACTION_VIEW, Uri.parse(firstUrl)))
                        }) {
                            Icon(Icons.Outlined.OpenInBrowser, contentDescription = null, modifier = Modifier.size(18.dp))
                            Spacer(Modifier.width(4.dp))
                            Text("Open URL")
                        }
                    }
                    FilledTonalButton(onClick = {
                        vm.createShare(memo.name) { url ->
                            if (url != null) {
                                val clipboard = context.getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
                                clipboard.setPrimaryClip(ClipData.newPlainText("memo share link", url))
                            }
                        }
                    }) {
                        Icon(Icons.Outlined.Share, contentDescription = null, modifier = Modifier.size(18.dp))
                        Spacer(Modifier.width(4.dp))
                        Text("Share")
                    }
                }
            } else {
                // Phone: icon-only row
                Row(
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(horizontal = 8.dp, vertical = 4.dp),
                    horizontalArrangement = Arrangement.SpaceEvenly,
                ) {
                    if (hasIncompleteTasks) {
                        IconButton(onClick = { showExtractSheet = true }, modifier = Modifier.semantics { contentDescription = "Extract tasks" }) {
                            Icon(Icons.Outlined.ChecklistRtl, contentDescription = null)
                        }
                    }
                    if (hasDate) {
                        IconButton(onClick = { showEventSheet = true }, modifier = Modifier.semantics { contentDescription = "Create event" }) {
                            Icon(Icons.Outlined.CalendarMonth, contentDescription = null)
                        }
                    }
                    if (hasGitea) {
                        IconButton(onClick = { showCommentIssueSheet = true }, modifier = Modifier.semantics { contentDescription = "Comment on issue" }) {
                            Icon(Icons.Outlined.Comment, contentDescription = null)
                        }
                    }
                    if (hasLink && firstUrl != null) {
                        IconButton(
                            onClick = { context.startActivity(Intent(Intent.ACTION_VIEW, Uri.parse(firstUrl))) },
                            modifier = Modifier.semantics { contentDescription = "Open URL" },
                        ) {
                            Icon(Icons.Outlined.OpenInBrowser, contentDescription = null)
                        }
                    }
                    IconButton(
                        onClick = {
                            vm.createShare(memo.name) { url ->
                                if (url != null) {
                                    val clipboard = context.getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
                                    clipboard.setPrimaryClip(ClipData.newPlainText("memo share link", url))
                                }
                            }
                        },
                        modifier = Modifier.semantics { contentDescription = "Share memo" },
                    ) {
                        Icon(Icons.Outlined.Share, contentDescription = null)
                    }
                }
            }
            HorizontalDivider(modifier = Modifier.padding(vertical = 4.dp))
        }

        // ── Attachments ───────────────────────────────────────────────────────
        if (memo.attachments.isNotEmpty()) {
            item {
                Text(
                    "Attachments",
                    style = MaterialTheme.typography.titleSmall,
                    modifier = Modifier.padding(horizontal = 16.dp, vertical = 4.dp),
                )
            }
            items(memo.attachments) { att ->
                val fileUrl = att.externalLink.ifBlank { "$memosHost/file/${att.name}/${att.filename}" }
                val isImage = att.type.let { t ->
                    t == "image/jpeg" || t == "image/png" || t == "image/gif" || t == "image/webp"
                } || att.filename.lowercase().let { it.endsWith(".jpg") || it.endsWith(".jpeg") || it.endsWith(".png") }

                Column(
                    modifier = Modifier
                        .fillMaxWidth()
                        .clickableWithoutRipple {
                            context.startActivity(Intent(Intent.ACTION_VIEW, Uri.parse(fileUrl)))
                        },
                ) {
                    if (isImage) {
                        MemoAuthImage(
                            url = fileUrl,
                            token = memosToken,
                            modifier = Modifier
                                .fillMaxWidth()
                                .height(160.dp)
                                .padding(horizontal = 16.dp)
                                .clip(MaterialTheme.shapes.small),
                        )
                        Spacer(Modifier.height(4.dp))
                    }
                    ListItem(
                        headlineContent = { Text(att.filename) },
                        supportingContent = { Text("${att.size / 1024} KB · ${att.type}") },
                        leadingContent = {
                            Icon(
                                if (isImage) Icons.Outlined.Image else Icons.Outlined.AttachFile,
                                contentDescription = null,
                            )
                        },
                    )
                }
            }
            item { HorizontalDivider(modifier = Modifier.padding(vertical = 4.dp)) }
        }

        // ── Comments ──────────────────────────────────────────────────────────
        item {
            Text(
                "Comments",
                style = MaterialTheme.typography.titleSmall,
                modifier = Modifier.padding(horizontal = 16.dp, vertical = 4.dp),
            )
        }
        if (commentsLoading) {
            item { CircularProgressIndicator(modifier = Modifier.padding(16.dp)) }
        } else {
            items(comments) { comment ->
                MemoCommentItem(comment = comment)
            }
        }
        // Comment input
        item {
            Row(
                modifier = Modifier
                    .padding(horizontal = 16.dp, vertical = 8.dp)
                    .fillMaxWidth(),
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.spacedBy(8.dp),
            ) {
                OutlinedTextField(
                    value = commentInput,
                    onValueChange = { commentInput = it },
                    modifier = Modifier.weight(1f),
                    placeholder = { Text("Add a comment…") },
                    maxLines = 3,
                )
                IconButton(
                    onClick = {
                        if (commentInput.isNotBlank()) {
                            onAddComment(commentInput)
                            commentInput = ""
                        }
                    },
                    modifier = Modifier.semantics { contentDescription = "Send comment" },
                ) {
                    Icon(Icons.Outlined.Send, contentDescription = null)
                }
            }
        }
    }

    // ── Action sheets ─────────────────────────────────────────────────────────
    if (showExtractSheet) {
        ExtractTasksConfirmSheet(
            parsedTasks = onExtractTasks(memo),
            onDismiss = { showExtractSheet = false },
            vm = vm,
        )
    }
    if (showEventSheet) {
        CreateEventFromMemoSheet(
            memo = memo,
            memosHost = memosHost,
            seedDate = onDetectDate(memo),
            onDismiss = { showEventSheet = false },
            vm = vm,
        )
    }
    if (showCommentIssueSheet) {
        CommentOnIssueSheet(
            memo = memo,
            memosHost = memosHost,
            issuesByRepo = issuesByRepo,
            onDismiss = { showCommentIssueSheet = false },
            vm = vm,
        )
    }
}

// Helper for clickable without ripple on ListItem
private fun Modifier.clickableWithoutRipple(onClick: () -> Unit): Modifier =
    this.then(androidx.compose.foundation.clickable(onClick = onClick))

// ─── Authenticated image loader ───────────────────────────────────────────────

@Composable
fun MemoAuthImage(url: String, token: String, modifier: Modifier = Modifier) {
    var state by remember(url) { mutableStateOf<AuthImageState>(AuthImageState.Loading) }

    LaunchedEffect(url) {
        state = AuthImageState.Loading
        state = withContext(Dispatchers.IO) {
            runCatching {
                val conn = URL(url).openConnection() as HttpURLConnection
                if (token.isNotBlank()) conn.setRequestProperty("Authorization", "Bearer $token")
                conn.connectTimeout = 10_000
                conn.readTimeout   = 15_000
                conn.connect()
                if (conn.responseCode == 200) {
                    conn.inputStream.use { BitmapFactory.decodeStream(it) }
                } else null
            }.getOrNull()
        }.let { bmp -> if (bmp != null) AuthImageState.Success(bmp) else AuthImageState.Error }
    }

    Box(modifier = modifier, contentAlignment = Alignment.Center) {
        when (val s = state) {
            AuthImageState.Loading -> CircularProgressIndicator(modifier = Modifier.size(28.dp))
            AuthImageState.Error   -> Icon(
                Icons.Outlined.BrokenImage,
                contentDescription = null,
                tint = MaterialTheme.colorScheme.onSurfaceVariant,
            )
            is AuthImageState.Success -> Image(
                bitmap = s.bitmap.asImageBitmap(),
                contentDescription = null,
                contentScale = ContentScale.Crop,
                modifier = Modifier.fillMaxSize(),
            )
        }
    }
}

private sealed interface AuthImageState {
    data object Loading : AuthImageState
    data object Error   : AuthImageState
    data class  Success(val bitmap: android.graphics.Bitmap) : AuthImageState
}
