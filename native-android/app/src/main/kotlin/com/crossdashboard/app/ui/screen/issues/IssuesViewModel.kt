package com.crossdashboard.app.ui.screen.issues

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.crossdashboard.app.data.prefs.CredentialKey
import com.crossdashboard.app.data.prefs.SecureStore
import com.crossdashboard.app.data.repository.IssueRepository
import com.crossdashboard.app.domain.model.GiteaComment
import com.crossdashboard.app.domain.model.GiteaIssue
import dagger.hilt.android.lifecycle.HiltViewModel
import kotlinx.coroutines.flow.*
import kotlinx.coroutines.launch
import javax.inject.Inject

enum class IssueStateFilter { OPEN, CLOSED, ALL }

data class IssuesUiState(
    val issues: List<GiteaIssue> = emptyList(),
    val filter: IssueStateFilter = IssueStateFilter.OPEN,
    val isLoading: Boolean = false,
    val error: String? = null,
    /** Comments keyed by issue id — loaded lazily on sheet open */
    val comments: Map<Long, List<GiteaComment>> = emptyMap(),
    val commentLoading: Set<Long> = emptySet(),
)

@HiltViewModel
class IssuesViewModel @Inject constructor(
    private val issueRepo: IssueRepository,
    private val secureStore: SecureStore,
) : ViewModel() {

    private val _filter = MutableStateFlow(IssueStateFilter.OPEN)
    private val _state = MutableStateFlow(IssuesUiState())
    val state: StateFlow<IssuesUiState> = _state.asStateFlow()

    init {
        viewModelScope.launch {
            combine(issueRepo.allIssues, _filter) { issues, filter ->
                when (filter) {
                    IssueStateFilter.OPEN -> issues.filter { it.state == "open" }
                    IssueStateFilter.CLOSED -> issues.filter { it.state == "closed" }
                    IssueStateFilter.ALL -> issues
                }.sortedByDescending { it.updatedAt }
            }.collect { filtered ->
                _state.update { it.copy(issues = filtered, filter = _filter.value) }
            }
        }
    }

    fun setFilter(filter: IssueStateFilter) {
        _filter.value = filter
    }

    fun sync() {
        viewModelScope.launch {
            _state.update { it.copy(isLoading = true, error = null) }
            try {
                val repos = secureStore.get(CredentialKey.GITEA_REPOS)
                    ?.split(",")?.map { it.trim() }?.filter { it.isNotBlank() }
                    ?: emptyList()
                issueRepo.sync(repos)
            } catch (e: Exception) {
                _state.update { it.copy(error = e.message) }
            } finally {
                _state.update { it.copy(isLoading = false) }
            }
        }
    }

    // ─── Comments ─────────────────────────────────────────────────────────────

    fun loadComments(issue: GiteaIssue) {
        if (issue.id in _state.value.comments) return  // already loaded
        viewModelScope.launch {
            _state.update { it.copy(commentLoading = it.commentLoading + issue.id) }
            try {
                val comments = issueRepo.fetchComments(issue.repository, issue.number)
                _state.update {
                    it.copy(
                        comments = it.comments + (issue.id to comments),
                        commentLoading = it.commentLoading - issue.id,
                    )
                }
            } catch (e: Exception) {
                _state.update {
                    it.copy(
                        error = e.message,
                        commentLoading = it.commentLoading - issue.id,
                    )
                }
            }
        }
    }

    fun addComment(issue: GiteaIssue, body: String) {
        if (body.isBlank()) return
        viewModelScope.launch {
            try {
                val comment = issueRepo.addComment(issue.repository, issue.number, body)
                val existing = _state.value.comments[issue.id] ?: emptyList()
                _state.update { it.copy(comments = it.comments + (issue.id to existing + comment)) }
            } catch (e: Exception) {
                _state.update { it.copy(error = e.message) }
            }
        }
    }

    // ─── State toggle ─────────────────────────────────────────────────────────

    fun toggleState(issue: GiteaIssue) {
        val newState = if (issue.state == "open") "closed" else "open"
        viewModelScope.launch {
            try {
                issueRepo.update(issue.repository, issue.number, state = newState)
            } catch (e: Exception) {
                _state.update { it.copy(error = e.message) }
            }
        }
    }

    // ─── Edit ─────────────────────────────────────────────────────────────────

    fun saveIssue(issue: GiteaIssue, title: String, body: String) {
        viewModelScope.launch {
            try {
                issueRepo.update(issue.repository, issue.number, title = title, body = body)
            } catch (e: Exception) {
                _state.update { it.copy(error = e.message) }
            }
        }
    }

    fun dismissError() = _state.update { it.copy(error = null) }
}
