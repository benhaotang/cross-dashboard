package com.crossdashboard.app.ui.screen.memos

import android.net.Uri
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyRow
import androidx.compose.foundation.lazy.items
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.outlined.AttachFile
import androidx.compose.material.icons.outlined.Close
import androidx.compose.material.icons.outlined.Send
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.unit.dp
import com.crossdashboard.app.data.repository.PendingAttachment
import com.crossdashboard.app.domain.model.MemoVisibility
import kotlinx.coroutines.launch

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun CreateMemoSheet(
    onDismiss: () -> Unit,
    onSubmit: (content: String, visibility: MemoVisibility, attachments: List<PendingAttachment>) -> Unit,
) {
    val context = LocalContext.current
    val scope = rememberCoroutineScope()
    var content by remember { mutableStateOf("") }
    var visibility by remember { mutableStateOf(MemoVisibility.PRIVATE) }
    var attachments by remember { mutableStateOf<List<PendingAttachment>>(emptyList()) }

    val filePicker = rememberLauncherForActivityResult(ActivityResultContracts.GetContent()) { uri: Uri? ->
        uri ?: return@rememberLauncherForActivityResult
        scope.launch {
            val bytes = context.contentResolver.openInputStream(uri)?.readBytes() ?: return@launch
            val type = context.contentResolver.getType(uri) ?: "application/octet-stream"
            val name = uri.lastPathSegment ?: "file"
            attachments = attachments + PendingAttachment(name, type, bytes)
        }
    }

    ModalBottomSheet(
        onDismissRequest = onDismiss,
        contentWindowInsets = { WindowInsets.ime },
    ) {
        Column(
            modifier = Modifier
                .padding(horizontal = 16.dp)
                .padding(bottom = 16.dp),
            verticalArrangement = Arrangement.spacedBy(12.dp),
        ) {
            Text("New Memo", style = MaterialTheme.typography.titleMedium)

            OutlinedTextField(
                value = content,
                onValueChange = { content = it },
                modifier = Modifier
                    .fillMaxWidth()
                    .heightIn(min = 120.dp),
                placeholder = { Text("What's on your mind?") },
                label = { Text("Content (Markdown)") },
            )

            // Visibility picker
            Text("Visibility", style = MaterialTheme.typography.labelMedium)
            SingleChoiceSegmentedButtonRow(modifier = Modifier.fillMaxWidth()) {
                MemoVisibility.entries.forEachIndexed { idx, vis ->
                    SegmentedButton(
                        shape = SegmentedButtonDefaults.itemShape(idx, MemoVisibility.entries.size),
                        selected = visibility == vis,
                        onClick = { visibility = vis },
                        label = { Text(vis.name.lowercase().replaceFirstChar { it.uppercaseChar() }) },
                    )
                }
            }

            // Attachments
            if (attachments.isNotEmpty()) {
                LazyRow(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    items(attachments) { att ->
                        InputChip(
                            selected = false,
                            onClick = {},
                            label = { Text(att.fileName, maxLines = 1) },
                            trailingIcon = {
                                IconButton(
                                    onClick = { attachments = attachments - att },
                                    modifier = Modifier.size(18.dp),
                                ) {
                                    Icon(Icons.Outlined.Close, contentDescription = "Remove attachment")
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
                IconButton(
                    onClick = { filePicker.launch("*/*") },
                    modifier = Modifier.semantics { contentDescription = "Attach file" },
                ) {
                    Icon(Icons.Outlined.AttachFile, contentDescription = null)
                }
                Spacer(Modifier.weight(1f))
                OutlinedButton(onClick = onDismiss) { Text("Cancel") }
                Button(
                    onClick = {
                        if (content.isNotBlank()) {
                            onSubmit(content, visibility, attachments)
                        }
                    },
                    enabled = content.isNotBlank(),
                ) {
                    Icon(Icons.Outlined.Send, contentDescription = null, modifier = Modifier.size(18.dp))
                    Spacer(Modifier.width(4.dp))
                    Text("Post")
                }
            }
        }
    }
}
