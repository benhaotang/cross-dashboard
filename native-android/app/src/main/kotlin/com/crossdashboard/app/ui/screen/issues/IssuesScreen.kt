package com.crossdashboard.app.ui.screen.issues

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.LazyRow
import androidx.compose.foundation.lazy.items
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
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.hilt.navigation.compose.hiltViewModel
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.crossdashboard.app.domain.model.GiteaIssue
import com.crossdashboard.app.ui.component.AdaptiveFilterBar
import com.crossdashboard.app.ui.component.AdaptiveFilterSpec
import com.crossdashboard.app.ui.component.FilterChoice
import com.crossdashboard.app.ui.component.TagFlow
import java.time.ZoneId
import java.time.format.DateTimeFormatter

@OptIn(ExperimentalMaterial3Api::class, ExperimentalMaterial3AdaptiveApi::class)
@Composable
fun IssuesScreen(
    initialIssueId: Long? = null,
    initialRepository: String? = null,
    viewModel: IssuesViewModel = hiltViewModel(),
) {
    val state by viewModel.state.collectAsStateWithLifecycle()
    val navigator = rememberListDetailPaneScaffoldNavigator<String>()
    var selectedIssue by remember { mutableStateOf<GiteaIssue?>(null) }
    val scope = rememberCoroutineScope()

    fun openIssue(issue: GiteaIssue) {
        selectedIssue = issue
        viewModel.loadComments(issue)
        if (navigator.scaffoldValue[ListDetailPaneScaffoldRole.Detail] != PaneAdaptedValue.Hidden) {
            scope.launch {
                navigator.navigateTo(ListDetailPaneScaffoldRole.Detail, issue.id.toString())
            }
        }
    }

    var initialNavigationDone by remember(initialIssueId, initialRepository) { mutableStateOf(false) }
    LaunchedEffect(initialIssueId, initialRepository, state.issues) {
        if (!initialNavigationDone && initialIssueId != null && state.issues.isNotEmpty()) {
            val target = state.issues.find {
                it.id == initialIssueId &&
                    (initialRepository == null || it.repository == initialRepository)
            }
            if (target != null) {
                selectedIssue = target
                viewModel.loadComments(target)
                if (navigator.scaffoldValue[ListDetailPaneScaffoldRole.Detail] != PaneAdaptedValue.Hidden) {
                    navigator.navigateTo(ListDetailPaneScaffoldRole.Detail, target.id.toString())
                }
                initialNavigationDone = true
            }
        }
    }

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
        floatingActionButton = {
            FloatingActionButton(
                onClick = { viewModel.showCreateSheet() },
                modifier = Modifier.semantics { contentDescription = "Create new issue" },
            ) {
                Icon(Icons.Outlined.Add, contentDescription = null)
            }
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
                        val issueFilters = buildList {
                            add(
                                AdaptiveFilterSpec(
                                    title = "Status",
                                    choices = IssueStateFilter.entries.map {
                                        FilterChoice(it.name, it.name.lowercase().replaceFirstChar { char -> char.uppercase() })
                                    },
                                    selectedKeys = setOf(state.filter.name),
                                    onSelectionChange = { selected ->
                                        selected.firstOrNull()?.let { viewModel.setFilter(IssueStateFilter.valueOf(it)) }
                                    },
                                )
                            )
                            add(
                                AdaptiveFilterSpec(
                                    title = "Tags",
                                    choices = state.availableLabels.map { FilterChoice(it, "#$it") },
                                    selectedKeys = state.selectedLabels,
                                    multiSelect = true,
                                    searchable = true,
                                    onSelectionChange = viewModel::setLabelFilters,
                                )
                            )
                            if (state.availableMilestones.isNotEmpty()) {
                                add(
                                    AdaptiveFilterSpec(
                                        title = "Milestone",
                                        choices = state.availableMilestones.map {
                                            FilterChoice(it.key, "${it.title} · ${it.repository.substringAfterLast('/')}")
                                        },
                                    selectedKeys = state.selectedMilestoneKey?.let(::setOf) ?: emptySet(),
                                    searchable = true,
                                    defaultKeys = emptySet(),
                                        onSelectionChange = { viewModel.setMilestoneFilter(it.firstOrNull()) },
                                    )
                                )
                            }
                        }
                        AdaptiveFilterBar(
                            filters = issueFilters,
                            hasActiveFilters = state.filter != IssueStateFilter.OPEN ||
                                state.selectedLabels.isNotEmpty() || state.selectedMilestoneKey != null,
                            onClear = viewModel::clearFilters,
                        )

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
                                        magicTags = state.magicTags,
                                        isSelected = navigator.currentDestination?.contentKey == issue.id.toString(),
                                        onClick = { openIssue(issue) },
                                    )
                                }
                            }
                        }
                    }
                }
            },
            detailPane = {
                AnimatedPane {
                    val issue = selectedIssue
                    if (issue != null) {
                        val comments = state.comments[issue.id] ?: emptyList()
                        val commentLoading = issue.id in state.commentLoading
                        IssueDetailContent(
                            issue = issue,
                            comments = comments,
                            commentLoading = commentLoading,
                            issueAttachments = state.issueAttachments[issue.id] ?: emptyList(),
                            commentAttachments = state.commentAttachments,
                            magicTags = state.magicTags,
                            onDismiss = { scope.launch { navigator.navigateBack() } },
                            onSave = { title, body, labels ->
                                viewModel.saveIssue(issue, title, body, labels)
                                scope.launch { navigator.navigateBack() }
                            },
                            onToggleState = {
                                viewModel.toggleState(issue)
                                scope.launch { navigator.navigateBack() }
                            },
                            onAddComment = { body, attachments -> viewModel.addComment(issue, body, attachments) },
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
            selectedIssue
        else null

    selectedIssueForSheet?.let { issue ->
        val comments = state.comments[issue.id] ?: emptyList()
        val commentLoading = issue.id in state.commentLoading
        IssuePropertySheet(
            issue = issue,
            comments = comments,
            commentLoading = commentLoading,
            issueAttachments = state.issueAttachments[issue.id] ?: emptyList(),
            commentAttachments = state.commentAttachments,
            magicTags = state.magicTags,
            onDismiss = { selectedIssue = null },
            onSave = { title, body, labels ->
                viewModel.saveIssue(issue, title, body, labels)
                selectedIssue = null
            },
            onToggleState = {
                viewModel.toggleState(issue)
                selectedIssue = null
            },
            onAddComment = { body, attachments -> viewModel.addComment(issue, body, attachments) },
        )
    }

    if (state.showCreateSheet) {
        CreateIssueSheet(
            repos = state.configuredRepos,
            isCreating = state.isCreating,
            onDismiss = { viewModel.dismissCreateSheet() },
            onCreate = { repo, title, body, attachments ->
                viewModel.createIssue(repo, title, body, attachments)
            },
        )
    }
}

// ─── Issue list row ───────────────────────────────────────────────────────────

private val updFmt = DateTimeFormatter.ofPattern("d MMM").withZone(ZoneId.systemDefault())

@Composable
private fun IssueListRow(
    issue: GiteaIssue,
    magicTags: List<String>,
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
                    TagFlow(
                        tags = issue.labels,
                        magicTags = magicTags,
                        modifier = Modifier.padding(top = 4.dp),
                    )
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
