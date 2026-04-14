package com.crossdashboard.app.ui.screen.memos

import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.LazyRow
import androidx.compose.foundation.lazy.items
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Add
import androidx.compose.material.icons.filled.PushPin
import androidx.compose.material.icons.outlined.PushPin
import androidx.compose.material3.*
import androidx.compose.material3.adaptive.layout.AnimatedPane
import androidx.compose.material3.adaptive.layout.ListDetailPaneScaffoldRole
import androidx.compose.material3.adaptive.navigation.NavigableListDetailPaneScaffold
import androidx.compose.material3.adaptive.navigation.rememberListDetailPaneScaffoldNavigator
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.hilt.navigation.compose.hiltViewModel
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import kotlinx.coroutines.launch
import com.crossdashboard.app.domain.model.MemosMemo
import com.crossdashboard.app.domain.model.MemoState
import com.crossdashboard.app.ui.navigation.Destination
import java.time.ZoneId
import java.time.format.DateTimeFormatter

@OptIn(ExperimentalMaterial3Api::class, ExperimentalFoundationApi::class)
@Composable
fun MemosScreen(
    onNavigate: (Destination) -> Unit = {},
    vm: MemosViewModel = hiltViewModel(),
) {
    val state by vm.state.collectAsStateWithLifecycle()
    val snackbarHostState = remember { SnackbarHostState() }

    LaunchedEffect(state.snackbarMessage) {
        state.snackbarMessage?.let {
            snackbarHostState.showSnackbar(it)
            vm.clearSnackbar()
        }
    }

    var selectedMemo by remember { mutableStateOf<MemosMemo?>(null) }
    var showCreateSheet by remember { mutableStateOf(false) }

    val navigator = rememberListDetailPaneScaffoldNavigator<MemosMemo>()
    val coroutineScope = rememberCoroutineScope()

    Scaffold(
        snackbarHost = { SnackbarHost(snackbarHostState) },
        floatingActionButton = {
            FloatingActionButton(
                onClick = { showCreateSheet = true },
                modifier = Modifier.semantics { contentDescription = "Create memo" },
            ) {
                Icon(Icons.Filled.Add, contentDescription = null)
            }
        },
    ) { innerPadding ->
        NavigableListDetailPaneScaffold(
            navigator = navigator,
            listPane = {
                AnimatedPane {
                    MemosListPane(
                        state = state,
                        onSelectMemo = { memo ->
                            selectedMemo = memo
                            coroutineScope.launch {
                                navigator.navigateTo(ListDetailPaneScaffoldRole.Detail, memo)
                            }
                        },
                        onSetStateFilter = vm::setStateFilter,
                        onSetTagFilter = vm::setTagFilter,
                        onSync = vm::sync,
                        onArchive = vm::archiveMemo,
                        onRestore = vm::restoreMemo,
                        onDelete = { vm.deleteMemo(it) },
                        modifier = Modifier.padding(innerPadding),
                    )
                }
            },
            detailPane = {
                AnimatedPane {
                    val memo = navigator.currentDestination?.contentKey ?: selectedMemo
                    if (memo != null) {
                        MemoPropertySheet(
                            memo = memo,
                            comments = state.comments[memo.name] ?: emptyList(),
                            commentsLoading = state.commentLoading.contains(memo.name),
                            memosHost = state.memosHost,
                            memosToken = state.memosToken,
                            issuesByRepo = state.issuesByRepo,
                            onLoadComments = { vm.loadComments(memo.name) },
                            onAddComment = { vm.createComment(memo.name, it) },
                            onShare = { vm.createShare(memo.name) {} },
                            onDelete = { vm.deleteMemo(memo.name) },
                            onArchive = { vm.archiveMemo(memo.name) },
                            onExtractTasks = vm::extractTasks,
                            onDetectDate = vm::detectFirstDate,
                            onFirstUrl = vm::firstUrl,
                            vm = vm,
                        )
                    } else {
                        Box(
                            Modifier
                                .fillMaxSize()
                                .padding(innerPadding),
                            contentAlignment = Alignment.Center,
                        ) {
                            Text("Select a memo", style = MaterialTheme.typography.bodyLarge, color = MaterialTheme.colorScheme.onSurfaceVariant)
                        }
                    }
                }
            },
        )
    }

    if (showCreateSheet) {
        CreateMemoSheet(
            onDismiss = { showCreateSheet = false },
            onSubmit = { content, visibility, attachments ->
                vm.createMemo(content, visibility, attachments)
                showCreateSheet = false
            },
        )
    }
}

// ─── List pane ────────────────────────────────────────────────────────────────

@Composable
private fun MemosListPane(
    state: MemosUiState,
    onSelectMemo: (MemosMemo) -> Unit,
    onSetStateFilter: (MemoStateFilter) -> Unit,
    onSetTagFilter: (String?) -> Unit,
    onSync: () -> Unit,
    onArchive: (String) -> Unit,
    onRestore: (String) -> Unit,
    onDelete: (String) -> Unit,
    modifier: Modifier = Modifier,
) {
    Column(modifier = modifier.fillMaxSize()) {
        // ── Filter bar ────────────────────────────────────────────────────────
        LazyRow(
            contentPadding = PaddingValues(horizontal = 16.dp, vertical = 8.dp),
            horizontalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            item {
                FilterChip(
                    selected = state.stateFilter == MemoStateFilter.NORMAL,
                    onClick = { onSetStateFilter(MemoStateFilter.NORMAL) },
                    label = { Text("Normal") },
                )
            }
            item {
                FilterChip(
                    selected = state.stateFilter == MemoStateFilter.ARCHIVED,
                    onClick = { onSetStateFilter(MemoStateFilter.ARCHIVED) },
                    label = { Text("Archived") },
                )
            }
            item {
                FilterChip(
                    selected = state.stateFilter == MemoStateFilter.ALL,
                    onClick = { onSetStateFilter(MemoStateFilter.ALL) },
                    label = { Text("All") },
                )
            }
            // Tag filters
            val tags = state.memos.flatMap { it.tags }.distinct()
            items(tags) { tag ->
                FilterChip(
                    selected = state.selectedTag == tag,
                    onClick = { onSetTagFilter(if (state.selectedTag == tag) null else tag) },
                    label = { Text("#$tag") },
                )
            }
        }

        if (state.isLoading) {
            LinearProgressIndicator(modifier = Modifier.fillMaxWidth())
        }

        if (state.memos.isEmpty() && !state.isLoading) {
            Box(Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
                Column(horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.spacedBy(8.dp)) {
                    Text("No memos", style = MaterialTheme.typography.bodyLarge, color = MaterialTheme.colorScheme.onSurfaceVariant)
                    TextButton(onClick = onSync) { Text("Sync") }
                }
            }
        } else {
            LazyColumn(
                contentPadding = PaddingValues(bottom = 88.dp),
            ) {
                items(state.memos, key = { it.name }) { memo ->
                    MemoListRow(
                        memo = memo,
                        onClick = { onSelectMemo(memo) },
                        onArchive = { if (memo.state == MemoState.ARCHIVED) onRestore(memo.name) else onArchive(memo.name) },
                        onDelete = { onDelete(memo.name) },
                    )
                    HorizontalDivider()
                }
            }
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun MemoListRow(
    memo: MemosMemo,
    onClick: () -> Unit,
    onArchive: () -> Unit,
    onDelete: () -> Unit,
) {
    val dismissState = rememberSwipeToDismissBoxState()
    LaunchedEffect(dismissState.currentValue) {
        when (dismissState.currentValue) {
            SwipeToDismissBoxValue.EndToStart -> onDelete()
            SwipeToDismissBoxValue.StartToEnd -> onArchive()
            else -> {}
        }
    }

    SwipeToDismissBox(
        state = dismissState,
        backgroundContent = {
            val color = when (dismissState.dismissDirection) {
                SwipeToDismissBoxValue.StartToEnd -> MaterialTheme.colorScheme.secondaryContainer
                SwipeToDismissBoxValue.EndToStart -> MaterialTheme.colorScheme.errorContainer
                else -> MaterialTheme.colorScheme.surface
            }
            Box(
                Modifier
                    .fillMaxSize()
                    .padding(horizontal = 16.dp),
                contentAlignment = if (dismissState.dismissDirection == SwipeToDismissBoxValue.StartToEnd)
                    Alignment.CenterStart else Alignment.CenterEnd,
            ) {
                Surface(color = color, modifier = Modifier.fillMaxSize()) {}
            }
        },
        content = {
            Surface(
                onClick = onClick,
                modifier = Modifier
                    .fillMaxWidth()
                    .semantics(mergeDescendants = true) { contentDescription = memo.snippet.ifBlank { memo.content }.take(80) },
            ) {
                Row(
                    modifier = Modifier.padding(horizontal = 16.dp, vertical = 12.dp),
                    verticalAlignment = Alignment.Top,
                    horizontalArrangement = Arrangement.spacedBy(12.dp),
                ) {
                    if (memo.pinned) {
                        Icon(Icons.Filled.PushPin, contentDescription = "Pinned", modifier = Modifier.size(16.dp), tint = MaterialTheme.colorScheme.primary)
                    }
                    Column(modifier = Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(4.dp)) {
                        if (memo.property.title.isNotBlank()) {
                            Text(memo.property.title, style = MaterialTheme.typography.titleSmall, maxLines = 1, overflow = TextOverflow.Ellipsis)
                        }
                        Text(
                            memo.snippet.ifBlank { memo.content }.take(200),
                            style = MaterialTheme.typography.bodyMedium,
                            maxLines = 3,
                            overflow = TextOverflow.Ellipsis,
                            color = MaterialTheme.colorScheme.onSurface,
                        )
                        if (memo.tags.isNotEmpty()) {
                            LazyRow(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                                items(memo.tags) { tag ->
                                    SuggestionChip(
                                        onClick = {},
                                        label = { Text("#$tag", style = MaterialTheme.typography.labelSmall) },
                                    )
                                }
                            }
                        }
                        Text(
                            relativeTime(memo.displayTime),
                            style = MaterialTheme.typography.labelSmall,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                        )
                    }
                }
            }
        },
    )
}

private fun relativeTime(instant: java.time.Instant): String {
    val now = java.time.Instant.now()
    val seconds = now.epochSecond - instant.epochSecond
    return when {
        seconds < 60    -> "just now"
        seconds < 3600  -> "${seconds / 60}m ago"
        seconds < 86400 -> "${seconds / 3600}h ago"
        else -> DateTimeFormatter.ofPattern("MMM d").withZone(ZoneId.systemDefault()).format(instant)
    }
}
