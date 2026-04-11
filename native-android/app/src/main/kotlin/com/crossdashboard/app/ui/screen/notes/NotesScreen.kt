package com.crossdashboard.app.ui.screen.notes

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.grid.GridCells
import androidx.compose.foundation.lazy.grid.LazyVerticalGrid
import androidx.compose.foundation.lazy.grid.items
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.outlined.Add
import androidx.compose.material.icons.outlined.Refresh
import androidx.compose.material.icons.outlined.Search
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
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.hilt.navigation.compose.hiltViewModel
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.crossdashboard.app.domain.model.Note
import com.crossdashboard.app.ui.component.CalendarColorDot
import com.crossdashboard.app.ui.component.CalendarColorResolver
import com.crossdashboard.app.ui.component.TagChip
import java.time.ZoneId
import java.time.format.DateTimeFormatter

@OptIn(ExperimentalMaterial3Api::class, ExperimentalMaterial3AdaptiveApi::class)
@Composable
fun NotesScreen(
    colorResolver: CalendarColorResolver? = null,
    viewModel: NotesViewModel = hiltViewModel(),
) {
    val state by viewModel.state.collectAsStateWithLifecycle()
    val navigator = rememberListDetailPaneScaffoldNavigator<String>()
    var selectedNote by remember { mutableStateOf<Note?>(null) }
    val scope = rememberCoroutineScope()
    var showCreateSheet by remember { mutableStateOf(false) }
    var showSearch by remember { mutableStateOf(false) }

    Scaffold(
        topBar = {
            if (showSearch) {
                SearchBar(
                    query = state.searchQuery,
                    onQueryChange = viewModel::onSearchChange,
                    onSearch = {},
                    active = true,
                    onActiveChange = { if (!it) showSearch = false },
                    placeholder = { Text("Search notes…") },
                    modifier = Modifier.fillMaxWidth(),
                ) {}
            } else {
                TopAppBar(
                    title = { Text("Notes") },
                    actions = {
                        IconButton(
                            onClick = { showSearch = true },
                            modifier = Modifier.semantics { contentDescription = "Search notes" },
                        ) {
                            Icon(Icons.Outlined.Search, contentDescription = null)
                        }
                        if (state.isLoading) {
                            CircularProgressIndicator(
                                modifier = Modifier.size(20.dp).padding(end = 4.dp),
                                strokeWidth = 2.dp,
                            )
                        }
                        IconButton(
                            onClick = { viewModel.sync() },
                            enabled = !state.isLoading,
                            modifier = Modifier.semantics { contentDescription = "Sync notes" },
                        ) {
                            Icon(Icons.Outlined.Refresh, contentDescription = null)
                        }
                    },
                )
            }
        },
        floatingActionButton = {
            FloatingActionButton(
                onClick = { showCreateSheet = true },
                modifier = Modifier.semantics { contentDescription = "Create new note" },
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
                    if (state.notes.isEmpty()) {
                        Box(
                            modifier = Modifier.fillMaxSize(),
                            contentAlignment = Alignment.Center,
                        ) {
                            Text(
                                if (state.searchQuery.isNotBlank()) "No matching notes"
                                else "No notes yet",
                                style = MaterialTheme.typography.bodyMedium,
                                color = MaterialTheme.colorScheme.onSurfaceVariant,
                            )
                        }
                    } else {
                        LazyVerticalGrid(
                            columns = GridCells.Adaptive(minSize = 160.dp),
                            contentPadding = PaddingValues(
                                start = 12.dp,
                                end = 12.dp,
                                top = 8.dp,
                                bottom = 100.dp,
                            ),
                            horizontalArrangement = Arrangement.spacedBy(8.dp),
                            verticalArrangement = Arrangement.spacedBy(8.dp),
                        ) {
                            items(state.notes, key = { it.uid }) { note ->
                                NoteCard(
                                    note = note,
                                    colorResolver = colorResolver,
                                    isSelected = navigator.currentDestination?.contentKey == note.uid,
                                    onClick = {
                                        selectedNote = note
                                        scope.launch {
                                            navigator.navigateTo(ListDetailPaneScaffoldRole.Detail, note.uid)
                                        }
                                    },
                                )
                            }
                        }
                    }
                }
            },
            detailPane = {
                AnimatedPane {
                    val note = selectedNote
                    if (note != null) {
                        NoteDetailContent(
                            note = note,
                            colorResolver = colorResolver,
                            onDismiss = { scope.launch { navigator.navigateBack() } },
                            onSave = { updated ->
                                viewModel.updateNote(updated)
                                scope.launch { navigator.navigateBack() }
                            },
                            onDelete = {
                                viewModel.deleteNote(note)
                                scope.launch { navigator.navigateBack() }
                            },
                        )
                    } else {
                        Box(Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
                            Text(
                                "Select a note",
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
    val selectedNoteForSheet =
        if (navigator.scaffoldValue[ListDetailPaneScaffoldRole.Detail] == PaneAdaptedValue.Hidden)
            selectedNote
        else null

    selectedNoteForSheet?.let { note ->
        NotePropertySheet(
            note = note,
            colorResolver = colorResolver,
            onDismiss = { scope.launch { navigator.navigateBack() } },
            onSave = { updated ->
                viewModel.updateNote(updated)
                scope.launch { navigator.navigateBack() }
            },
            onDelete = {
                viewModel.deleteNote(note)
                scope.launch { navigator.navigateBack() }
            },
        )
    }

    if (showCreateSheet) {
        CreateNoteSheet(
            onDismiss = { showCreateSheet = false },
            onCreate = { summary, body, tags ->
                viewModel.createNote(summary, body, tags)
                showCreateSheet = false
            },
        )
    }
}

// ─── Note card ────────────────────────────────────────────────────────────────

private val modFmt = DateTimeFormatter.ofPattern("d MMM").withZone(ZoneId.systemDefault())

@Composable
private fun NoteCard(
    note: Note,
    colorResolver: CalendarColorResolver?,
    isSelected: Boolean = false,
    onClick: () -> Unit,
) {
    ElevatedCard(
        onClick = onClick,
        modifier = Modifier
            .fillMaxWidth()
            .semantics {
                contentDescription = buildString {
                    append("Note: ${note.summary}. ")
                    if (note.categories.isNotEmpty()) append("Tags: ${note.categories.joinToString()}. ")
                    if (note.body.isNotBlank()) append(note.body.take(80))
                }
            },
        elevation = CardDefaults.elevatedCardElevation(
            defaultElevation = if (isSelected) 6.dp else 1.dp,
        ),
        colors = CardDefaults.elevatedCardColors(
            containerColor = if (isSelected)
                MaterialTheme.colorScheme.secondaryContainer.copy(alpha = 0.5f)
            else MaterialTheme.colorScheme.surface,
        ),
    ) {
        Column(modifier = Modifier.padding(12.dp)) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Text(
                    text = note.summary,
                    style = MaterialTheme.typography.titleSmall,
                    fontWeight = FontWeight.SemiBold,
                    maxLines = 2,
                    overflow = TextOverflow.Ellipsis,
                    modifier = Modifier.weight(1f),
                )
                if (colorResolver != null) {
                    CalendarColorDot(
                        calendarHref = note.calendarHref,
                        resolver = colorResolver,
                        modifier = Modifier.padding(start = 4.dp),
                    )
                }
            }

            if (note.body.isNotBlank()) {
                Spacer(Modifier.height(4.dp))
                Text(
                    text = note.body,
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                    maxLines = 4,
                    overflow = TextOverflow.Ellipsis,
                )
            }

            if (note.categories.isNotEmpty()) {
                Spacer(Modifier.height(6.dp))
                Row(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                    note.categories.take(3).forEach { tag ->
                        TagChip(label = "#$tag")
                    }
                }
            }

            Spacer(Modifier.height(6.dp))
            Text(
                text = modFmt.format(note.lastModified),
                style = MaterialTheme.typography.labelSmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant.copy(alpha = 0.6f),
            )
        }
    }
}

// ─── Create note sheet ────────────────────────────────────────────────────────

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun CreateNoteSheet(
    onDismiss: () -> Unit,
    onCreate: (String, String, List<String>) -> Unit,
) {
    val sheetState = rememberModalBottomSheetState(skipPartiallyExpanded = true)
    var title by remember { mutableStateOf("") }
    var body by remember { mutableStateOf("") }
    var tagsInput by remember { mutableStateOf("") }

    ModalBottomSheet(
        onDismissRequest = onDismiss,
        sheetState = sheetState,
        contentWindowInsets = { WindowInsets.ime },
    ) {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = 20.dp)
                .navigationBarsPadding(),
            verticalArrangement = Arrangement.spacedBy(12.dp),
        ) {
            Text(
                text = "New Note",
                style = MaterialTheme.typography.titleMedium,
                fontWeight = FontWeight.SemiBold,
            )

            OutlinedTextField(
                value = title,
                onValueChange = { title = it },
                label = { Text("Title") },
                modifier = Modifier.fillMaxWidth(),
            )

            OutlinedTextField(
                value = body,
                onValueChange = { body = it },
                label = { Text("Content") },
                modifier = Modifier.fillMaxWidth(),
                minLines = 4,
                maxLines = 10,
            )

            OutlinedTextField(
                value = tagsInput,
                onValueChange = { tagsInput = it },
                label = { Text("Tags (comma-separated)") },
                modifier = Modifier.fillMaxWidth(),
            )

            Button(
                onClick = {
                    val tags = tagsInput.split(",").map { it.trim() }.filter { it.isNotBlank() }
                    onCreate(title.trim(), body.trim(), tags)
                },
                enabled = title.isNotBlank(),
                modifier = Modifier
                    .fillMaxWidth()
                    .semantics { contentDescription = "Create note" },
            ) {
                Text("Create")
            }

            Spacer(Modifier.height(8.dp))
        }
    }
}
