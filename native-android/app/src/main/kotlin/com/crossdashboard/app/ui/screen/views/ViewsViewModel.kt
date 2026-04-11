package com.crossdashboard.app.ui.screen.views

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.crossdashboard.app.data.prefs.AppPreferences
import com.crossdashboard.app.data.repository.IssueRepository
import com.crossdashboard.app.data.repository.TaskRepository
import com.crossdashboard.app.domain.model.*
import dagger.hilt.android.lifecycle.HiltViewModel
import kotlinx.coroutines.flow.*
import kotlinx.coroutines.launch
import javax.inject.Inject

enum class ViewMode { KANBAN, COVEY }

/** Covey four-quadrant tag names (fixed). */
object CoveyTag {
    const val DO = "do"
    const val DELAY = "delay"
    const val DELEGATE = "delegate"
    const val ELIMINATE = "eliminate"

    val ALL = listOf(DO, DELAY, DELEGATE, ELIMINATE)
}

/**
 * A unified item combining either a [CalDavTask] or a [GiteaIssue] for display in Views.
 * This avoids Kotlin sealed-class boxing — every item carries both optional fields.
 */
data class ViewItem(
    val key: String,
    val title: String,
    val labels: List<String>,       // categories (task) or label names (issue)
    val isTask: Boolean,
    val priority: Int = 0,          // from task.priority; 0 for issues
    val due: java.time.Instant? = null,
    val taskRef: CalDavTask? = null,
    val issueRef: GiteaIssue? = null,
)

data class ViewsUiState(
    val items: List<ViewItem> = emptyList(),
    val kanbanColumns: List<String> = DEFAULT_KANBAN_COLUMNS,
    val viewMode: ViewMode = ViewMode.KANBAN,
    /** When non-null, the assign modal is shown for this item (tap-card flow) */
    val assigningItem: ViewItem? = null,
    /**
     * When non-null, the bulk assign modal is open; value is the currently selected
     * destination column/quadrant tag. The modal lists all items not yet in that destination.
     */
    val bulkAssignTarget: String? = null,
    /** Editable column name list shown in the config modal */
    val editingColumns: List<String>? = null,
    val isLoading: Boolean = false,
    val error: String? = null,
)

@HiltViewModel
class ViewsViewModel @Inject constructor(
    private val taskRepo: TaskRepository,
    private val issueRepo: IssueRepository,
    private val prefs: AppPreferences,
) : ViewModel() {

    private val _state = MutableStateFlow(ViewsUiState())
    val state: StateFlow<ViewsUiState> = _state.asStateFlow()

    init {
        viewModelScope.launch {
            combine(
                taskRepo.allTasks,
                issueRepo.allIssues,
                prefs.kanbanColumnsFlow,
            ) { tasks, issues, columns ->
                val taskItems = tasks
                    .filter { it.status != TaskStatus.COMPLETED && it.status != TaskStatus.CANCELLED }
                    .map { t ->
                        ViewItem(
                            key = "task_${t.uid}",
                            title = t.summary,
                            labels = t.categories,
                            isTask = true,
                            priority = t.priority,
                            due = t.due,
                            taskRef = t,
                        )
                    }
                val issueItems = issues
                    .filter { it.state == "open" }
                    .map { i ->
                        ViewItem(
                            key = "issue_${i.id}",
                            title = i.title,
                            labels = i.labels,
                            isTask = false,
                            issueRef = i,
                        )
                    }
                Triple(taskItems + issueItems, columns, _state.value.viewMode)
            }.collect { (items, columns, mode) ->
                _state.update { it.copy(items = items, kanbanColumns = columns, viewMode = mode) }
            }
        }
    }

    // ─── View mode ────────────────────────────────────────────────────────────

    fun setViewMode(mode: ViewMode) = _state.update { it.copy(viewMode = mode) }

    // ─── Assign modal ─────────────────────────────────────────────────────────

    fun openAssign(item: ViewItem) = _state.update { it.copy(assigningItem = item) }
    fun closeAssign() = _state.update { it.copy(assigningItem = null) }

    /**
     * Assign [tag] to [item], removing any other tags from [exclusiveSet] first
     * (mutual exclusivity within the current view's tag set).
     */
    fun assignTag(item: ViewItem, tag: String, exclusiveSet: List<String>) {
        viewModelScope.launch {
            val newLabels = item.labels
                .filterNot { it in exclusiveSet }  // remove all existing view-tags
                .plus(tag)                          // add the chosen one

            try {
                if (item.isTask && item.taskRef != null) {
                    val updated = item.taskRef.copy(categories = newLabels)
                    taskRepo.update(updated)
                } else if (!item.isTask && item.issueRef != null) {
                    issueRepo.replaceLabels(
                        item.issueRef.repository,
                        item.issueRef.number,
                        newLabels,
                    )
                }
            } catch (e: Exception) {
                _state.update { it.copy(error = e.message) }
            } finally {
                closeAssign()
            }
        }
    }

    /** Remove [tag] from [item] without adding another. */
    fun removeTag(item: ViewItem, tag: String) {
        viewModelScope.launch {
            val newLabels = item.labels.filterNot { it == tag }
            try {
                if (item.isTask && item.taskRef != null) {
                    taskRepo.update(item.taskRef.copy(categories = newLabels))
                } else if (!item.isTask && item.issueRef != null) {
                    issueRepo.replaceLabels(
                        item.issueRef.repository,
                        item.issueRef.number,
                        newLabels,
                    )
                }
            } catch (e: Exception) {
                _state.update { it.copy(error = e.message) }
            }
        }
    }

    // ─── Bulk assign modal (FAB / per-column + button flow) ───────────────────

    /**
     * Open the bulk assign modal pre-targeted at [targetTag]. If null, defaults to the
     * first kanban column (or first Covey quadrant when in Covey mode).
     */
    fun openBulkAssign(targetTag: String? = null) {
        val s = _state.value
        val target = targetTag ?: when (s.viewMode) {
            ViewMode.KANBAN -> s.kanbanColumns.firstOrNull()
            ViewMode.COVEY -> CoveyTag.ALL.first()
        } ?: return
        _state.update { it.copy(bulkAssignTarget = target) }
    }

    fun closeBulkAssign() = _state.update { it.copy(bulkAssignTarget = null) }

    fun setBulkAssignTarget(tag: String) = _state.update { it.copy(bulkAssignTarget = tag) }

    /**
     * Assign [tag] to [item] without closing the bulk assign modal, so the user can
     * keep assigning additional items in the same session.
     */
    fun assignTagFromBulk(item: ViewItem, tag: String) {
        viewModelScope.launch {
            val exclusiveSet = when (_state.value.viewMode) {
                ViewMode.KANBAN -> _state.value.kanbanColumns
                ViewMode.COVEY -> CoveyTag.ALL
            }
            val newLabels = item.labels.filterNot { it in exclusiveSet }.plus(tag)
            try {
                if (item.isTask && item.taskRef != null) {
                    taskRepo.update(item.taskRef.copy(categories = newLabels))
                } else if (!item.isTask && item.issueRef != null) {
                    issueRepo.replaceLabels(item.issueRef.repository, item.issueRef.number, newLabels)
                }
            } catch (e: Exception) {
                _state.update { it.copy(error = e.message) }
            }
            // Modal intentionally stays open for batch assignment
        }
    }

    // ─── Column config ────────────────────────────────────────────────────────

    fun openColumnConfig() {
        _state.update { it.copy(editingColumns = it.kanbanColumns.toMutableList()) }
    }

    fun closeColumnConfig() = _state.update { it.copy(editingColumns = null) }

    fun saveColumns(columns: List<String>) {
        val trimmed = columns.map { it.trim().lowercase() }.filter { it.isNotBlank() }
        if (trimmed.isEmpty()) return
        viewModelScope.launch {
            prefs.setKanbanColumns(trimmed)
            _state.update { it.copy(editingColumns = null) }
        }
    }

    fun dismissError() = _state.update { it.copy(error = null) }
}
