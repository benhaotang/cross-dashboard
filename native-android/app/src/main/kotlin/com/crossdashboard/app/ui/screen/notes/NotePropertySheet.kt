package com.crossdashboard.app.ui.screen.notes

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.unit.dp
import com.crossdashboard.app.domain.model.Note
import com.crossdashboard.app.ui.component.*
import java.time.ZoneId
import java.time.format.DateTimeFormatter

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun NotePropertySheet(
    note: Note,
    colorResolver: CalendarColorResolver?,
    onDismiss: () -> Unit,
    onSave: (Note) -> Unit,
    onDelete: () -> Unit,
    /** When true, renders content inline (no ModalBottomSheet wrapper) for tablet detail pane. */
    inlineMode: Boolean = false,
) {
    var editing by remember(note.uid) { mutableStateOf(false) }
    var editedNote by remember(note.uid) { mutableStateOf(note) }

    @Composable
    fun SheetContent() {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .verticalScroll(rememberScrollState())
                .then(if (!inlineMode) Modifier.navigationBarsPadding() else Modifier),
        ) {
            PropertySheetHeader(
                title = if (editing) "Edit Note" else note.summary,
                editing = editing,
                onEditToggle = {
                    if (editing) editedNote = note  // cancel: reset
                    editing = !editing
                },
                onClose = null,
            )

            if (editing) {
                NoteEditForm(
                    note = editedNote,
                    onNoteChange = { editedNote = it },
                    onSave = {
                        onSave(editedNote)
                        editing = false
                    },
                    onDelete = onDelete,
                )
            } else {
                NoteReadView(
                    note = note,
                    colorResolver = colorResolver,
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

// ─── Read view ────────────────────────────────────────────────────────────────

private val dtFmt = DateTimeFormatter.ofPattern("d MMM yyyy, HH:mm")
    .withZone(ZoneId.systemDefault())

@Composable
internal fun NoteReadView(
    note: Note,
    colorResolver: CalendarColorResolver?,
) {
    if (note.categories.isNotEmpty()) {
        TagFlow(
            tags = note.categories,
            modifier = Modifier.padding(horizontal = 20.dp, vertical = 4.dp),
        )
    }

    if (note.body.isNotBlank()) {
        MarkdownText(
            content = note.body,
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = 20.dp, vertical = 8.dp),
        )
    }

    ReadField(label = "Created", value = dtFmt.format(note.created))
    ReadField(label = "Modified", value = dtFmt.format(note.lastModified))

    colorResolver?.displayName(note.calendarHref)?.let { name ->
        ReadField(label = "Calendar", value = name)
    }

    Spacer(Modifier.height(16.dp))
}

// ─── Inline detail content (tablet / NavigableListDetailPaneScaffold) ─────────

/**
 * Renders the full note detail inline, without a bottom sheet wrapper.
 * Used in the detail pane of [NotesScreen] on tablet/Expanded windows.
 */
@Composable
fun NoteDetailContent(
    note: Note,
    colorResolver: CalendarColorResolver?,
    onDismiss: () -> Unit = {},
    onSave: (Note) -> Unit,
    onDelete: () -> Unit,
) {
    NotePropertySheet(
        note = note,
        colorResolver = colorResolver,
        onDismiss = onDismiss,
        onSave = onSave,
        onDelete = onDelete,
        inlineMode = true,
    )
}

// ─── Edit form ────────────────────────────────────────────────────────────────

@Composable
internal fun NoteEditForm(
    note: Note,
    onNoteChange: (Note) -> Unit,
    onSave: () -> Unit,
    onDelete: () -> Unit,
) {
    var showDeleteDialog by remember { mutableStateOf(false) }
    var tagsInput by remember(note.uid) {
        mutableStateOf(note.categories.joinToString(", "))
    }

    Column(
        modifier = Modifier.padding(horizontal = 20.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp),
    ) {
        OutlinedTextField(
            value = note.summary,
            onValueChange = { onNoteChange(note.copy(summary = it)) },
            label = { Text("Title") },
            modifier = Modifier.fillMaxWidth(),
        )

        OutlinedTextField(
            value = note.body,
            onValueChange = { onNoteChange(note.copy(body = it)) },
            label = { Text("Content") },
            modifier = Modifier.fillMaxWidth(),
            minLines = 5,
            maxLines = 15,
        )

        OutlinedTextField(
            value = tagsInput,
            onValueChange = { raw ->
                tagsInput = raw
                val tags = raw.split(",").map { it.trim() }.filter { it.isNotBlank() }
                onNoteChange(note.copy(categories = tags))
            },
            label = { Text("Tags (comma-separated)") },
            modifier = Modifier.fillMaxWidth(),
        )

        Button(
            onClick = onSave,
            modifier = Modifier
                .fillMaxWidth()
                .semantics { contentDescription = "Save note changes" },
        ) {
            Text("Save")
        }

        OutlinedButton(
            onClick = { showDeleteDialog = true },
            modifier = Modifier
                .fillMaxWidth()
                .semantics { contentDescription = "Delete this note" },
            colors = ButtonDefaults.outlinedButtonColors(
                contentColor = MaterialTheme.colorScheme.error,
            ),
        ) {
            Text("Delete Note")
        }
    }

    if (showDeleteDialog) {
        AlertDialog(
            onDismissRequest = { showDeleteDialog = false },
            title = { Text("Delete Note") },
            text = { Text("Delete \u201c${note.summary}\u201d? This cannot be undone.") },
            confirmButton = {
                TextButton(onClick = {
                    showDeleteDialog = false
                    onDelete()
                }) { Text("Delete", color = MaterialTheme.colorScheme.error) }
            },
            dismissButton = {
                TextButton(onClick = { showDeleteDialog = false }) { Text("Cancel") }
            },
        )
    }
}
