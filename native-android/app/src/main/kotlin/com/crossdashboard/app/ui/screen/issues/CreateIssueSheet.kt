package com.crossdashboard.app.ui.screen.issues

import android.provider.OpenableColumns
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.outlined.AttachFile
import androidx.compose.material.icons.outlined.Close
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.unit.dp
import com.crossdashboard.app.ui.component.PropertySheet
import com.crossdashboard.app.ui.component.PropertySheetHeader
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun CreateIssueSheet(
    repos: List<String>,
    isCreating: Boolean,
    onDismiss: () -> Unit,
    onCreate: (repo: String, title: String, body: String, attachments: List<PendingAttachment>) -> Unit,
) {
    val sheetState = rememberModalBottomSheetState(skipPartiallyExpanded = true)
    PropertySheet(onDismiss = onDismiss, sheetState = sheetState) {
        CreateIssueContent(
            repos = repos,
            isCreating = isCreating,
            onDismiss = onDismiss,
            onCreate = onCreate,
        )
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun CreateIssueContent(
    repos: List<String>,
    isCreating: Boolean,
    onDismiss: () -> Unit,
    onCreate: (repo: String, title: String, body: String, attachments: List<PendingAttachment>) -> Unit,
) {
    val context = LocalContext.current
    val scope = rememberCoroutineScope()

    var selectedRepo by remember { mutableStateOf(repos.firstOrNull() ?: "") }
    var title by remember { mutableStateOf("") }
    var body by remember { mutableStateOf("") }
    var pendingAttachments by remember { mutableStateOf<List<PendingAttachment>>(emptyList()) }
    var repoMenuExpanded by remember { mutableStateOf(false) }

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

    Column(
        modifier = Modifier
            .fillMaxWidth()
            .navigationBarsPadding(),
    ) {
        PropertySheetHeader(
            title = "New Issue",
            editing = false,
            onEditToggle = {},
            onClose = onDismiss,
        )

        Column(
            modifier = Modifier
                .fillMaxWidth()
                .verticalScroll(rememberScrollState())
                .padding(horizontal = 20.dp),
            verticalArrangement = Arrangement.spacedBy(12.dp),
        ) {
            // Repository selector
            if (repos.size > 1) {
                ExposedDropdownMenuBox(
                    expanded = repoMenuExpanded,
                    onExpandedChange = { repoMenuExpanded = it },
                ) {
                    OutlinedTextField(
                        value = selectedRepo,
                        onValueChange = {},
                        readOnly = true,
                        label = { Text("Repository") },
                        trailingIcon = { ExposedDropdownMenuDefaults.TrailingIcon(repoMenuExpanded) },
                        modifier = Modifier
                            .fillMaxWidth()
                            .menuAnchor(ExposedDropdownMenuAnchorType.PrimaryNotEditable),
                    )
                    ExposedDropdownMenu(
                        expanded = repoMenuExpanded,
                        onDismissRequest = { repoMenuExpanded = false },
                    ) {
                        repos.forEach { repo ->
                            DropdownMenuItem(
                                text = { Text(repo) },
                                onClick = {
                                    selectedRepo = repo
                                    repoMenuExpanded = false
                                },
                            )
                        }
                    }
                }
            } else if (repos.size == 1) {
                OutlinedTextField(
                    value = selectedRepo,
                    onValueChange = {},
                    readOnly = true,
                    label = { Text("Repository") },
                    modifier = Modifier.fillMaxWidth(),
                    enabled = false,
                )
            } else {
                OutlinedTextField(
                    value = selectedRepo,
                    onValueChange = { selectedRepo = it },
                    label = { Text("Repository (owner/repo)") },
                    modifier = Modifier.fillMaxWidth(),
                    singleLine = true,
                )
            }

            OutlinedTextField(
                value = title,
                onValueChange = { title = it },
                label = { Text("Title *") },
                modifier = Modifier.fillMaxWidth(),
                singleLine = true,
                isError = title.isBlank(),
            )

            OutlinedTextField(
                value = body,
                onValueChange = { body = it },
                label = { Text("Description (optional)") },
                modifier = Modifier.fillMaxWidth(),
                minLines = 4,
                maxLines = 10,
            )

            // Pending attachments
            if (pendingAttachments.isNotEmpty()) {
                Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
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

            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.spacedBy(8.dp),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                OutlinedButton(
                    onClick = { fileLauncher.launch("*/*") },
                    modifier = Modifier.semantics { contentDescription = "Attach file" },
                ) {
                    Icon(
                        Icons.Outlined.AttachFile,
                        contentDescription = null,
                        modifier = Modifier.size(16.dp),
                    )
                    Spacer(Modifier.width(4.dp))
                    Text("Attach")
                }

                Button(
                    onClick = { onCreate(selectedRepo, title, body, pendingAttachments) },
                    enabled = title.isNotBlank() && selectedRepo.isNotBlank() && !isCreating,
                    modifier = Modifier
                        .weight(1f)
                        .semantics { contentDescription = "Submit new issue" },
                ) {
                    if (isCreating) {
                        CircularProgressIndicator(
                            modifier = Modifier.size(16.dp),
                            strokeWidth = 2.dp,
                            color = MaterialTheme.colorScheme.onPrimary,
                        )
                        Spacer(Modifier.width(8.dp))
                    }
                    Text("Create Issue")
                }
            }

            Spacer(Modifier.height(8.dp))
        }
    }
}
