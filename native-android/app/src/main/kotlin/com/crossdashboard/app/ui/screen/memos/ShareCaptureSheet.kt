package com.crossdashboard.app.ui.screen.memos

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyRow
import androidx.compose.foundation.lazy.items
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.outlined.Close
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.crossdashboard.app.data.repository.PendingAttachment
import com.crossdashboard.app.domain.model.MemoVisibility

/**
 * Bottom-sheet overlay shown when the app is launched via a system share intent.
 * Pre-fills the Memos compose area with shared text + attachments.
 * If the shared text looks like a Memos share URL ({host}/s/{token}), the caller
 * should resolve it via getMemoByShare() and navigate directly instead.
 */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun ShareCaptureSheet(
    sharedText: String?,
    sharedAttachments: List<PendingAttachment>,
    onDismiss: () -> Unit,
    onCapture: (content: String, visibility: MemoVisibility, attachments: List<PendingAttachment>) -> Unit,
) {
    var content by remember { mutableStateOf(sharedText ?: "") }
    var visibility by remember { mutableStateOf(MemoVisibility.PRIVATE) }
    val attachments = remember { mutableStateListOf(*sharedAttachments.toTypedArray()) }

    ModalBottomSheet(onDismissRequest = onDismiss, windowInsets = WindowInsets.ime) {
        Column(
            modifier = Modifier.padding(horizontal = 16.dp).padding(bottom = 24.dp),
            verticalArrangement = Arrangement.spacedBy(12.dp),
        ) {
            Text("Capture to Memos", style = MaterialTheme.typography.titleMedium)

            OutlinedTextField(
                value = content,
                onValueChange = { content = it },
                modifier = Modifier.fillMaxWidth().heightIn(min = 80.dp),
                label = { Text("Content") },
                placeholder = { Text("Add more details…") },
            )

            // Visibility
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

            // Attachment preview
            if (attachments.isNotEmpty()) {
                LazyRow(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    items(attachments) { att ->
                        InputChip(
                            selected = false,
                            onClick = {},
                            label = { Text(att.fileName, maxLines = 1) },
                            trailingIcon = {
                                IconButton(onClick = { attachments.remove(att) }, modifier = Modifier.size(18.dp)) {
                                    Icon(Icons.Outlined.Close, contentDescription = "Remove")
                                }
                            },
                        )
                    }
                }
            }

            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.spacedBy(8.dp, androidx.compose.ui.Alignment.End),
            ) {
                OutlinedButton(onClick = onDismiss) { Text("Cancel") }
                Button(
                    onClick = { onCapture(content, visibility, attachments.toList()); onDismiss() },
                    enabled = content.isNotBlank(),
                ) { Text("Capture to Memos") }
            }
        }
    }
}
