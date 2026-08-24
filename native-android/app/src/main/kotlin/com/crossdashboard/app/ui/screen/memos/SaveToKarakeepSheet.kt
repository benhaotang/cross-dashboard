package com.crossdashboard.app.ui.screen.memos

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.imePadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.outlined.BookmarkAdd
import androidx.compose.material3.Button
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.ListItem
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.ModalBottomSheet
import androidx.compose.material3.RadioButton
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import com.crossdashboard.app.data.network.KarakeepFolder

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun SaveToKarakeepSheet(
    urls: List<String>,
    folders: List<KarakeepFolder>,
    loading: Boolean,
    saving: Boolean,
    error: String?,
    onLoadFolders: () -> Unit,
    onSave: (String?) -> Unit,
    onDismiss: () -> Unit,
) {
    var selectedFolderId by remember { mutableStateOf<String?>(null) }
    LaunchedEffect(Unit) { onLoadFolders() }

    ModalBottomSheet(onDismissRequest = onDismiss) {
        Column(
            modifier = Modifier.padding(horizontal = 16.dp).padding(bottom = 16.dp).imePadding(),
            verticalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {                                        androidx.compose.material3.Icon(Icons.Outlined.BookmarkAdd, contentDescription = null)
                Text(if (urls.size == 1) "Save link to Karakeep" else "Save ${urls.size} links to Karakeep")
            }
            urls.take(3).forEach { url ->
                Text(url, maxLines = 1, overflow = TextOverflow.Ellipsis)
            }
            if (urls.size > 3) Text("+${urls.size - 3} more")

            Text("Folder")
            FolderChoice("No folder", null, selectedFolderId) { selectedFolderId = null }
            if (loading) {
                CircularProgressIndicator(modifier = Modifier.size(24.dp))
            } else {
                folders.forEach { folder ->
                    FolderChoice(folderLabel(folder, folders), folder.id, selectedFolderId) { selectedFolderId = folder.id }
                }
            }
            error?.let { Text(it, color = MaterialTheme.colorScheme.error) }

            Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                TextButton(onClick = onDismiss, modifier = Modifier.weight(1f)) { Text("Cancel") }
                Button(
                    onClick = { onSave(selectedFolderId) },
                    enabled = !loading && !saving,
                    modifier = Modifier.weight(1f),
                ) { Text("Save") }
            }
        }
    }
}

private fun folderLabel(folder: KarakeepFolder, folders: List<KarakeepFolder>): String {
    val names = mutableListOf(folder.name)
    val byId = folders.associateBy { it.id }
    val visited = mutableSetOf(folder.id)
    var parentId = folder.parentId
    while (parentId != null && visited.add(parentId)) {
        val parent = byId[parentId] ?: break
        names.add(0, parent.name)
        parentId = parent.parentId                                                               }
    return names.joinToString(" / ")
}

@Composable
private fun FolderChoice(label: String, id: String?, selectedId: String?, onSelect: () -> Unit) {
    ListItem(
        headlineContent = { Text(label) },
        leadingContent = { RadioButton(selected = id == selectedId, onClick = onSelect) },           modifier = Modifier.fillMaxWidth(),
    )
}
