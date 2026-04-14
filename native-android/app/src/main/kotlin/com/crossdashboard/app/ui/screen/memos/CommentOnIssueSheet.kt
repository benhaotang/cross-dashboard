package com.crossdashboard.app.ui.screen.memos

import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.crossdashboard.app.domain.model.GiteaIssue
import com.crossdashboard.app.domain.model.MemosMemo

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun CommentOnIssueSheet(
    memo: MemosMemo,
    memosHost: String,
    issuesByRepo: Map<String, List<GiteaIssue>>,
    onDismiss: () -> Unit,
    vm: MemosViewModel,
) {
    val repos = issuesByRepo.keys.toList()
    val memoUrl = if (memosHost.isNotBlank()) "$memosHost/${memo.name}" else ""
    var selectedRepo by remember { mutableStateOf(repos.firstOrNull() ?: "") }
    var selectedIssue by remember { mutableStateOf<GiteaIssue?>(null) }
    var body by remember { mutableStateOf("${memo.snippet.take(500)}\n\n${memoUrl}".trim()) }
    var repoExpanded by remember { mutableStateOf(false) }
    var issueExpanded by remember { mutableStateOf(false) }

    val repoIssues = issuesByRepo[selectedRepo] ?: emptyList()

    ModalBottomSheet(onDismissRequest = onDismiss, windowInsets = WindowInsets.ime) {
        Column(
            modifier = Modifier.padding(horizontal = 16.dp).padding(bottom = 24.dp),
            verticalArrangement = Arrangement.spacedBy(12.dp),
        ) {
            Text("Comment on Issue", style = MaterialTheme.typography.titleMedium)

            // ── Repository picker ─────────────────────────────────────────────
            ExposedDropdownMenuBox(expanded = repoExpanded, onExpandedChange = { repoExpanded = it }) {
                OutlinedTextField(
                    value = selectedRepo,
                    onValueChange = {},
                    readOnly = true,
                    label = { Text("Repository") },
                    trailingIcon = { ExposedDropdownMenuDefaults.TrailingIcon(repoExpanded) },
                    modifier = Modifier.menuAnchor(MenuAnchorType.PrimaryNotEditable).fillMaxWidth(),
                )
                ExposedDropdownMenu(expanded = repoExpanded, onDismissRequest = { repoExpanded = false }) {
                    repos.forEach { repo ->
                        DropdownMenuItem(
                            text = { Text(repo) },
                            onClick = {
                                selectedRepo = repo
                                selectedIssue = null
                                repoExpanded = false
                            },
                        )
                    }
                }
            }

            // ── Issue picker ──────────────────────────────────────────────────
            if (repoIssues.isEmpty()) {
                Text(
                    if (selectedRepo.isEmpty()) "No Gitea repository configured — add one in Settings."
                    else "No open issues synced for "$selectedRepo". Run a sync first.",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            } else {
                ExposedDropdownMenuBox(expanded = issueExpanded, onExpandedChange = { issueExpanded = it }) {
                    OutlinedTextField(
                        value = selectedIssue?.let { "#${it.number}  ${it.title}" } ?: "",
                        onValueChange = {},
                        readOnly = true,
                        label = { Text("Issue") },
                        placeholder = { Text("Select an issue…") },
                        trailingIcon = { ExposedDropdownMenuDefaults.TrailingIcon(issueExpanded) },
                        modifier = Modifier.menuAnchor(MenuAnchorType.PrimaryNotEditable).fillMaxWidth(),
                    )
                    ExposedDropdownMenu(
                        expanded = issueExpanded,
                        onDismissRequest = { issueExpanded = false },
                    ) {
                        repoIssues.forEach { issue ->
                            DropdownMenuItem(
                                text = { Text("#${issue.number}  ${issue.title}") },
                                onClick = { selectedIssue = issue; issueExpanded = false },
                            )
                        }
                    }
                }
            }

            // ── Comment body ──────────────────────────────────────────────────
            OutlinedTextField(
                value = body,
                onValueChange = { body = it },
                label = { Text("Comment") },
                modifier = Modifier.fillMaxWidth().heightIn(min = 100.dp),
                minLines = 3,
            )

            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.spacedBy(8.dp, Alignment.End),
            ) {
                OutlinedButton(onClick = onDismiss) { Text("Cancel") }
                Button(
                    onClick = {
                        val issue = selectedIssue ?: return@Button
                        vm.addCommentToIssue(selectedRepo, issue.number, body)
                        onDismiss()
                    },
                    enabled = selectedRepo.isNotBlank() && selectedIssue != null && body.isNotBlank(),
                ) { Text("Comment") }
            }
        }
    }
}
