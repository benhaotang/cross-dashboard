package com.crossdashboard.app.ui.screen.tasks

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.crossdashboard.app.data.parser.TaskInputParser
import com.crossdashboard.app.data.prefs.AppPreferences
import com.crossdashboard.app.data.prefs.CredentialKey
import com.crossdashboard.app.data.prefs.SecureStore
import com.crossdashboard.app.data.repository.TaskRepository
import com.crossdashboard.app.domain.model.CalDavCalendar
import com.crossdashboard.app.domain.model.CalDavTask
import com.crossdashboard.app.domain.model.ParsedTask
import com.crossdashboard.app.domain.model.TaskDefaults
import com.crossdashboard.app.domain.model.TaskStatus
import kotlinx.serialization.json.Json
import dagger.hilt.android.lifecycle.HiltViewModel
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.flow.*
import kotlinx.coroutines.launch
import java.time.Instant
import javax.inject.Inject

enum class TaskFilter { ALL, ACTIVE, COMPLETED }

data class TasksUiState(
    val tasks: List<CalDavTask> = emptyList(),
    val rootTasks: List<CalDavTask> = emptyList(),
    val filter: TaskFilter = TaskFilter.ACTIVE,
    val quickInput: String = "",
    val parsedPreview: ParsedTask? = null,
    val autoFocusInput: Boolean = false,
    val isLoading: Boolean = false,
    val error: String? = null,
    val kanbanColumns: List<String> = listOf("backlog", "planned", "inprogress", "done"),
    val availableTags: List<String> = emptyList(),
    val selectedTags: Set<String> = emptySet(),
    val defaultCalendarHref: String? = null,
    /** uid → expanded state for the subtask tree */
    val expandedUids: Set<String> = emptySet(),
)

@OptIn(ExperimentalCoroutinesApi::class)
@HiltViewModel
class TasksViewModel @Inject constructor(
    private val taskRepo: TaskRepository,
    private val prefs: AppPreferences,
    private val secureStore: SecureStore,
) : ViewModel() {

    private val _state = MutableStateFlow(TasksUiState())
    val state: StateFlow<TasksUiState> = _state.asStateFlow()

    private val _filter = MutableStateFlow(TaskFilter.ACTIVE)
    private val _selectedTags = MutableStateFlow<Set<String>>(emptySet())

    /** Map of parentUid → subtask list (loaded lazily when a node is expanded) */
    private val subtaskCache = mutableMapOf<String, List<CalDavTask>>()

    init {
        // Combine filter with the appropriate task flow
        viewModelScope.launch {
            combine(taskRepo.allTasks, _filter, _selectedTags) { allTasks, filter, selectedTags ->
                val tasks = allTasks
                    .filter { it.matchesStatus(filter) }
                    .filter { task -> selectedTags.all { it in task.categories } }
                // Only root tasks (no parent) form the tree roots
                val roots = tasks.filter { it.parentUid == null }
                Triple(allTasks, roots, selectedTags)
            }.collect { (allTasks, roots, selectedTags) ->
                _state.update {
                    it.copy(
                        tasks = allTasks,
                        rootTasks = roots,
                        filter = _filter.value,
                        availableTags = allTasks.flatMap { task -> task.categories }
                            .distinct().sortedBy { tag -> tag.lowercase() },
                        selectedTags = selectedTags,
                    )
                }
            }
        }

        viewModelScope.launch {
            prefs.kanbanColumnsFlow.collect { cols ->
                _state.update { it.copy(kanbanColumns = cols) }
            }
        }

        viewModelScope.launch {
            _state.update {
                it.copy(defaultCalendarHref = secureStore.get(CredentialKey.CALDAV_DEFAULT_TASK_CALENDAR))
            }
        }
    }

    // ─── Filter ───────────────────────────────────────────────────────────────

    fun setFilter(filter: TaskFilter) {
        _filter.value = filter
    }

    fun setTagFilters(tags: Set<String>) {
        _selectedTags.value = tags
    }

    fun clearFilters() {
        _filter.value = TaskFilter.ACTIVE
        _selectedTags.value = emptySet()
    }

    // ─── Quick input ──────────────────────────────────────────────────────────

    fun onQuickInputChange(text: String) {
        _state.update { it.copy(quickInput = text) }
        if (text.isBlank()) {
            _state.update { it.copy(parsedPreview = null) }
            return
        }
        val defaults = runCatching {
            TaskDefaults()   // loaded from prefs in the background; default values used for preview
        }.getOrDefault(TaskDefaults())
        _state.update { it.copy(parsedPreview = TaskInputParser.parse(text, defaults)) }
    }

    fun submitQuickInput() {
        val input = _state.value.quickInput.trim()
        if (input.isBlank()) return
        val calendarHref = _state.value.defaultCalendarHref ?: return

        viewModelScope.launch {
            _state.update { it.copy(isLoading = true, error = null) }
            try {
                val defaults = TaskDefaults()
                val parsed = TaskInputParser.parse(input, defaults)
                val task = CalDavTask(
                    uid = java.util.UUID.randomUUID().toString(),
                    summary = parsed.summary,
                    priority = parsed.priority,
                    categories = parsed.categories,
                    due = parsed.due,
                    created = Instant.now(),
                    lastModified = Instant.now(),
                )
                taskRepo.create(task, calendarHref)
                _state.update { it.copy(quickInput = "", parsedPreview = null) }
            } catch (e: Exception) {
                _state.update { it.copy(error = e.message) }
            } finally {
                _state.update { it.copy(isLoading = false) }
            }
        }
    }

    fun setAutoFocus(focus: Boolean) = _state.update { it.copy(autoFocusInput = focus) }

    // ─── Completion toggle ────────────────────────────────────────────────────

    fun toggleComplete(task: CalDavTask) {
        viewModelScope.launch {
            try {
                taskRepo.toggleComplete(task)
            } catch (e: Exception) {
                _state.update { it.copy(error = e.message) }
            }
        }
    }

    // ─── Subtask expand / collapse ────────────────────────────────────────────

    fun toggleExpand(uid: String) {
        val current = _state.value.expandedUids
        if (uid in current) {
            _state.update { it.copy(expandedUids = current - uid) }
        } else {
            _state.update { it.copy(expandedUids = current + uid) }
            // Eagerly load subtasks if not yet cached
            if (uid !in subtaskCache) {
                viewModelScope.launch {
                    taskRepo.subtasksOf(uid).first().let { subtaskCache[uid] = it }
                }
            }
        }
    }

    fun subtasksOf(uid: String): Flow<List<CalDavTask>> =
        combine(taskRepo.subtasksOf(uid), _filter, _selectedTags) { tasks, filter, selectedTags ->
            tasks.filter { it.matchesStatus(filter) }
                .filter { task -> selectedTags.all { it in task.categories } }
        }

    private fun CalDavTask.matchesStatus(filter: TaskFilter): Boolean = when (filter) {
        TaskFilter.ALL -> true
        TaskFilter.ACTIVE -> status != TaskStatus.COMPLETED && status != TaskStatus.CANCELLED
        TaskFilter.COMPLETED -> status == TaskStatus.COMPLETED
    }

    // ─── CRUD ─────────────────────────────────────────────────────────────────

    fun saveTask(task: CalDavTask) {
        viewModelScope.launch {
            try {
                taskRepo.update(task)
            } catch (e: Exception) {
                _state.update { it.copy(error = e.message) }
            }
        }
    }

    fun deleteTask(task: CalDavTask) {
        viewModelScope.launch {
            try {
                taskRepo.delete(task)
            } catch (e: Exception) {
                _state.update { it.copy(error = e.message) }
            }
        }
    }

    fun sync() {
        viewModelScope.launch {
            _state.update { it.copy(isLoading = true, error = null) }
            try {
                val raw = secureStore.get(CredentialKey.CALDAV_SELECTED_CALENDARS) ?: return@launch
                val hrefs = runCatching {
                    Json.decodeFromString<List<CalDavCalendar>>(raw).map { it.href }
                }.getOrDefault(emptyList())
                taskRepo.sync(hrefs)
            } catch (e: Exception) {
                _state.update { it.copy(error = e.message) }
            } finally {
                _state.update { it.copy(isLoading = false) }
            }
        }
    }

    fun dismissError() = _state.update { it.copy(error = null) }
}
