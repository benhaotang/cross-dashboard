package com.crossdashboard.app.ui.screen.issues

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material.icons.Icons
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
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.hilt.navigation.compose.hiltViewModel
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.crossdashboard.app.domain.model.GiteaIssue
import java.time.ZoneId
import java.time.format.DateTimeFormatter

@OptIn(ExperimentalMaterial3Api::class, ExperimentalMaterial3AdaptiveApi::class)
@Composable
fun IssuesScreen(
    viewModel: IssuesViewModel = hiltViewModel(),
) {
    val state by viewModel.state.collectAsStateWithLifecycle()
    val navigator = rememberListDetailPaneScaffoldNavigator<GiteaIssue>()
    val scope = rememberCoroutineScope()

    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text("Issues") },
                actions = {
                    if (state.isLoading) {
                        CircularProgressIndicator(
                            modifier = Modifier.size(20.dp).padding(end = 4.dp),
                            strokeWidth = 2.dp,
                        )
                    }
                    IconButton(
                        onClick = { viewModel.sync() },
                        enabled = !state.isLoading,
                        modifier = Modifier.semantics { contentDescription = "Sync issues" },
                    ) {
                        Icon(Icons.Outlined.Refresh, contentDescription = null)
                    }
                },
            )
        },
    ) { paddingValues ->
        NavigableListDetailPaneScaffold(
            navigator = navigator,
            modifier = Modifier
                .fillMaxSize()
                .padding(paddingValues),
            listPane = {
                AnimatedPane {
                    Column(modifier = Modifier.fillMaxSize()) {
                        // ── State filter chips ────────────────────────────────
                        Row(
                            modifier = Modifier
                                .fillMaxWidth()
                                .padding(horizontal = 12.dp, vertical = 4.dp),
                            horizontalArrangement = Arrangement.spacedBy(6.dp),
                        ) {
                            IssueStateFilter.entries.forEach { filter ->
                                FilterChip(
                                    selected = state.filter == filter,
                                    onClick = { viewModel.setFilter(filter) },
                                    label = {
                                        Text(
                                            text = filter.name.lowercase()
                                                .replaceFirstChar { it.uppercase() },
                                            style = MaterialTheme.typography.labelMedium,
                                        )
                                    },
                                )
                            }
                        }

                        if (state.issues.isEmpty()) {
                            Box(Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
                                Text(
                                    "No ${state.filter.name.lowercase()} issues",
                                    style = MaterialTheme.typography.bodyMedium,
                                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                                )
                            }
                        } else {
                            LazyColumn(
                                contentPadding = PaddingValues(bottom = 80.dp),
                                verticalArrangement = Arrangement.spacedBy(2.dp),
                            ) {
                                items(state.issues, key = { it.id }) { issue ->
                                    IssueListRow(
                                        issue = issue,
                        isSelected = navigator.currentDestination?.contentKey?.id == issue.id,
                                    onClick = {
                                        viewModel.loadComments(issue)
                                        scope.launch {
                                            navigator.navigateTo(ListDetailPaneScaffoldRole.Detail, issue)
                                        }
                                    },
                                    )
                                }
                            }
                        }
                    }
                }
            },
            detailPane = {
                AnimatedPane {
                    val selectedIssue = navigator.currentDestination?.contentKey
                    if (selectedIssue != null) {
                        val comments = state.comments[selectedIssue.id] ?: emptyList()
                        val commentLoading = selectedIssue.id in state.commentLoading
                        IssueDetailContent(
                            issue = selectedIssue,
                            comments = comments,
                            commentLoading = commentLoading,
                            onDismiss = { scope.launch { navigator.navigateBack() } },
                            onSave = { title, body ->
                                viewModel.saveIssue(selectedIssue, title, body)
                                scope.launch { navigator.navigateBack() }
                            },
                            onToggleState = {
                                viewModel.toggleState(selectedIssue)
                                scope.launch { navigator.navigateBack() }
                            },
                            onAddComment = { body -> viewModel.addComment(selectedIssue, body) },
                        )
                    } else {
                        Box(Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
                            Text(
                                "Select an issue",
                                style = MaterialTheme.typography.bodyMedium,
                                color = MaterialTheme.colorScheme.onSurfaceVariant,
                            )
                        }
                    }
                }
            },
        )
    }

    // Phone: show bottom sheet when detail pane is hidden (single-pane)
    val selectedIssueForSheet =
        if (navigator.scaffoldValue[ListDetailPaneScaffoldRole.Detail] == PaneAdaptedValue.Hidden)
            navigator.currentDestination?.contentKey
        else null

    selectedIssueForSheet?.let { issue ->
        val comments = state.comments[issue.id] ?: emptyList()
        val commentLoading = issue.id in state.commentLoading
        IssuePropertySheet(
            issue = issue,
            comments = comments,
            commentLoading = commentLoading,
            onDismiss = { scope.launch { navigator.navigateBack() } },
            onSave = { title, body ->
                viewModel.saveIssue(issue, title, body)
                scope.launch { navigator.navigateBack() }
            },
            onToggleState = {
                viewModel.toggleState(issue)
                scope.launch { navigator.navigateBack() }
            },
            onAddComment = { body -> viewModel.addComment(issue, body) },
        )
    }
}

// ─── Issue list row ───────────────────────────────────────────────────────────

private val updFmt = DateTimeFormatter.ofPattern("d MMM").withZone(ZoneId.systemDefault())

@Composable
private fun IssueListRow(
    issue: GiteaIssue,
    isSelected: Boolean = false,
    onClick: () -> Unit,
) {
    val isOpen = issue.state == "open"
    Surface(
        onClick = onClick,
        modifier = Modifier
            .fillMaxWidth()
            .semantics {
                contentDescription = buildString {
                    append("Issue #${issue.number}: ${issue.title}. ")
                    append("Status: ${if (isOpen) "open" else "closed"}. ")
                    if (issue.labels.isNotEmpty()) append("Labels: ${issue.labels.joinToString()}. ")
                }
            },
        color = if (isSelected) MaterialTheme.colorScheme.secondaryContainer.copy(alpha = 0.4f)
                else Color.Transparent,
    ) {
        Row(
            modifier = Modifier.padding(horizontal = 16.dp, vertical = 10.dp),
            verticalAlignment = Alignment.Top,
        ) {
            // State indicator dot
            Surface(
                shape = MaterialTheme.shapes.extraSmall,
                color = if (isOpen) MaterialTheme.colorScheme.primaryContainer
                else MaterialTheme.colorScheme.surfaceVariant,
                modifier = Modifier
                    .padding(top = 3.dp, end = 10.dp)
                    .size(8.dp),
            ) {}

            Column(modifier = Modifier.weight(1f)) {
                Text(
                    text = issue.title,
                    style = MaterialTheme.typography.bodyMedium,
                    fontWeight = FontWeight.Medium,
                    maxLines = 2,
                )
                Row(
                    horizontalArrangement = Arrangement.spacedBy(6.dp),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Text(
                        text = "#${issue.number} · ${issue.repository.substringAfterLast('/')}",
                        style = MaterialTheme.typography.labelSmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                    Text(
                        text = "· ${updFmt.format(issue.updatedAt)}",
                        style = MaterialTheme.typography.labelSmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                }
                if (issue.labels.isNotEmpty()) {
                    Row(
                        horizontalArrangement = Arrangement.spacedBy(4.dp),
                        modifier = Modifier.padding(top = 4.dp),
                    ) {
                        issue.labels.take(3).forEach { label ->
                            SuggestionChip(
                                onClick = {},
                                label = { Text(label, style = MaterialTheme.typography.labelSmall) },
                            )
                        }
                    }
                }
            }
        }
    }

    HorizontalDivider(
        modifier = Modifier.padding(start = 34.dp),
        color = MaterialTheme.colorScheme.outlineVariant.copy(alpha = 0.4f),
        thickness = 0.5.dp,
    )
}
