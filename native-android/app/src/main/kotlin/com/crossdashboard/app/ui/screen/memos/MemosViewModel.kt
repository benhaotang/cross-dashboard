package com.crossdashboard.app.ui.screen.memos

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.crossdashboard.app.data.parser.TaskInputParser
import com.crossdashboard.app.data.prefs.CredentialKey
import com.crossdashboard.app.data.prefs.SecureStore
import com.crossdashboard.app.data.repository.EventRepository
import com.crossdashboard.app.data.repository.IssueRepository
import com.crossdashboard.app.data.repository.MemoRepository
import com.crossdashboard.app.data.repository.PendingAttachment
import com.crossdashboard.app.data.repository.TaskRepository
import com.crossdashboard.app.domain.model.*
import dagger.hilt.android.lifecycle.HiltViewModel
import kotlinx.coroutines.flow.*
import kotlinx.coroutines.launch
import java.time.Instant
import java.time.ZoneId
import java.time.format.DateTimeFormatter
import javax.inject.Inject

enum class MemoStateFilter { NORMAL, ARCHIVED, ALL }

data class MemosUiState(
    val memos: List<MemosMemo> = emptyList(),
    val stateFilter: MemoStateFilter = MemoStateFilter.NORMAL,
    val availableTags: List<String> = emptyList(),
    val selectedTags: Set<String> = emptySet(),
    val dateRangeStart: Instant? = null,
    val dateRangeEnd: Instant? = null,
    val isLoading: Boolean = false,
    val error: String? = null,
    val comments: Map<String, List<MemosMemo>> = emptyMap(),   // keyed by memo name
    val commentLoading: Set<String> = emptySet(),
    val snackbarMessage: String? = null,
    val configuredRepos: List<String> = emptyList(),
    val memosHost: String = "",
    val memosToken: String = "",
    /** Open issues from Room, grouped by "owner/repo", used by CommentOnIssueSheet. */
    val issuesByRepo: Map<String, List<GiteaIssue>> = emptyMap(),
)

@HiltViewModel
class MemosViewModel @Inject constructor(
    private val memoRepo: MemoRepository,
    private val taskRepo: TaskRepository,
    private val eventRepo: EventRepository,
    private val issueRepo: IssueRepository,
    private val secureStore: SecureStore,
) : ViewModel() {

    private val _state = MutableStateFlow(MemosUiState())
    val state: StateFlow<MemosUiState> = _state.asStateFlow()

    init {
        val host  = secureStore.get(CredentialKey.MEMOS_HOST)  ?: ""
        val token = secureStore.get(CredentialKey.MEMOS_TOKEN) ?: ""
        val repos = secureStore.get(CredentialKey.GITEA_REPOS)
            ?.split(",")?.map { it.trim() }?.filter { it.isNotBlank() }
            ?: emptyList()
        _state.update { it.copy(memosHost = host, memosToken = token, configuredRepos = repos) }

        viewModelScope.launch {
            memoRepo.allMemos.collect { all ->
                applyFilters(all)
            }
        }

        viewModelScope.launch {
            issueRepo.allIssues.collect { all ->
                _state.update { it.copy(
                    issuesByRepo = all.filter { issue -> issue.state == "open" }
                        .groupBy { issue -> issue.repository }
                ) }
            }
        }
    }

    private fun applyFilters(all: List<MemosMemo>) {
        val s = _state.value
        var filtered = when (s.stateFilter) {
            MemoStateFilter.NORMAL   -> all.filter { it.state == MemoState.NORMAL }
            MemoStateFilter.ARCHIVED -> all.filter { it.state == MemoState.ARCHIVED }
            MemoStateFilter.ALL      -> all
        }
        filtered = filtered.filter { memo -> s.selectedTags.all { it in memo.tags } }
        s.dateRangeStart?.let { start -> filtered = filtered.filter { it.displayTime >= start } }
        s.dateRangeEnd?.let { end -> filtered = filtered.filter { it.displayTime <= end } }
        _state.update {
            it.copy(
                memos = filtered,
                availableTags = all.flatMap { memo -> memo.tags }
                    .distinct().sortedBy { tag -> tag.lowercase() },
            )
        }
    }

    fun setStateFilter(filter: MemoStateFilter) {
        _state.update { it.copy(stateFilter = filter) }
        viewModelScope.launch {
            applyFilters(memoRepo.allMemos.first())
        }
    }

    fun setTagFilters(tags: Set<String>) {
        _state.update { it.copy(selectedTags = tags) }
        viewModelScope.launch {
            applyFilters(memoRepo.allMemos.first())
        }
    }

    fun clearFilters() {
        _state.update {
            it.copy(stateFilter = MemoStateFilter.NORMAL, selectedTags = emptySet())
        }
        viewModelScope.launch { applyFilters(memoRepo.allMemos.first()) }
    }

    fun setDateRange(start: Instant?, end: Instant?) {
        _state.update { it.copy(dateRangeStart = start, dateRangeEnd = end) }
        viewModelScope.launch {
            applyFilters(memoRepo.allMemos.first())
        }
    }

    fun sync() {
        viewModelScope.launch {
            _state.update { it.copy(isLoading = true, error = null) }
            runCatching { memoRepo.syncMemos() }
                .onFailure { e -> _state.update { it.copy(error = e.message) } }
            _state.update { it.copy(isLoading = false) }
        }
    }

    fun createMemo(
        content: String,
        visibility: MemoVisibility,
        attachments: List<PendingAttachment>,
    ) {
        if (content.isBlank()) return
        viewModelScope.launch {
            _state.update { it.copy(isLoading = true, error = null) }
            val created = runCatching {
                memoRepo.createMemo(content, visibility, attachments)
            }.getOrNull()
            if (created != null) {
                _state.update { it.copy(snackbarMessage = "Memo created") }
            } else {
                _state.update { it.copy(error = "Failed to create memo") }
            }
            _state.update { it.copy(isLoading = false) }
        }
    }

    fun deleteMemo(name: String, force: Boolean = false) {
        viewModelScope.launch {
            memoRepo.deleteMemo(name, force)
            _state.update { it.copy(snackbarMessage = "Memo deleted") }
        }
    }

    fun archiveMemo(name: String) {
        viewModelScope.launch {
            memoRepo.archiveMemo(name)
            _state.update { it.copy(snackbarMessage = "Memo archived") }
        }
    }

    fun restoreMemo(name: String) {
        viewModelScope.launch {
            memoRepo.restoreMemo(name)
            _state.update { it.copy(snackbarMessage = "Memo restored") }
        }
    }

    fun loadComments(memoName: String) {
        if (_state.value.commentLoading.contains(memoName)) return
        viewModelScope.launch {
            _state.update { it.copy(commentLoading = it.commentLoading + memoName) }
            val comments = runCatching { memoRepo.loadComments(memoName) }.getOrDefault(emptyList())
            _state.update {
                it.copy(
                    comments = it.comments + (memoName to comments),
                    commentLoading = it.commentLoading - memoName,
                )
            }
        }
    }

    fun createComment(parentName: String, content: String) {
        if (content.isBlank()) return
        viewModelScope.launch {
            runCatching { memoRepo.createComment(parentName, content) }
            loadComments(parentName)
        }
    }

    fun createShare(memoId: String, onResult: (String?) -> Unit) {
        viewModelScope.launch {
            val url = memoRepo.createShare(memoId)
            onResult(url)
        }
    }

    // ─── Action helpers ───────────────────────────────────────────────────────

    /** Extract markdown task items from memo content. */
    fun extractTasks(memo: MemosMemo): List<ParsedTask> {
        return memo.content
            .lines()
            .filter { it.trimStart().startsWith("- [ ]") }
            .mapNotNull { line ->
                val raw = line.trimStart().removePrefix("- [ ]").trim()
                if (raw.isNotBlank()) TaskInputParser.parse(raw) else null
            }
    }

    /** Detect dates/keywords in memo content for event creation seeding. */
    fun detectFirstDate(memo: MemosMemo): Instant? {
        val keywords = mapOf(
            "today" to 0L, "tomorrow" to 1L,
            "monday" to null, "tuesday" to null, "wednesday" to null,
            "thursday" to null, "friday" to null, "saturday" to null, "sunday" to null,
        )
        val lower = memo.content.lowercase()
        for ((kw, offset) in keywords) {
            if (lower.contains(kw)) {
                return if (offset != null) {
                    Instant.now().plusSeconds(offset * 86400)
                } else {
                    Instant.now()
                }
            }
        }
        // ISO date: 2025-12-25
        val isoRegex = Regex("""\d{4}-\d{2}-\d{2}""")
        isoRegex.find(memo.content)?.let {
            return runCatching {
                java.time.LocalDate.parse(it.value)
                    .atStartOfDay(ZoneId.systemDefault())
                    .toInstant()
            }.getOrNull()
        }
        return null
    }

    /** Return first URL found in memo content, or null. */
    fun firstUrl(memo: MemosMemo): String? {
        val urlRegex = Regex("""https?://\S+""")
        return urlRegex.find(memo.content)?.value
    }

    fun createTaskFromParsed(parsed: ParsedTask) {
        viewModelScope.launch {
            val calendarHref = secureStore.get(CredentialKey.CALDAV_DEFAULT_TASK_CALENDAR) ?: return@launch
            runCatching {
                taskRepo.create(
                    CalDavTask(
                        uid = java.util.UUID.randomUUID().toString(),
                        summary = parsed.summary,
                        priority = parsed.priority,
                        categories = parsed.categories,
                        due = parsed.due,
                    ),
                    calendarHref,
                )
            }
        }
    }

    fun createEventFromMemo(
        summary: String,
        description: String,
        start: Instant,
        end: Instant,
        calendarHref: String,
    ) {
        viewModelScope.launch {
            runCatching {
                eventRepo.create(
                    CalendarEvent(
                        uid = java.util.UUID.randomUUID().toString(),
                        summary = summary,
                        start = start,
                        end = end,
                        description = description,
                    ),
                    calendarHref,
                )
            }.onSuccess { _state.update { it.copy(snackbarMessage = "Event created") } }
                .onFailure { e -> _state.update { it.copy(error = e.message) } }
        }
    }

    fun addCommentToIssue(repo: String, issueNumber: Int, body: String) {
        viewModelScope.launch {
            runCatching { issueRepo.addComment(repo, issueNumber, body) }
                .onSuccess { _state.update { it.copy(snackbarMessage = "Comment added") } }
                .onFailure { e -> _state.update { it.copy(error = e.message) } }
        }
    }

    fun clearSnackbar() = _state.update { it.copy(snackbarMessage = null) }
    fun showSnackbar(msg: String) = _state.update { it.copy(snackbarMessage = msg) }
    fun clearError() = _state.update { it.copy(error = null) }
}
