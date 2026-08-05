package com.crossdashboard.app.ui.screen.issues

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.crossdashboard.app.data.prefs.CredentialKey
import com.crossdashboard.app.data.prefs.SecureStore
import com.crossdashboard.app.data.repository.IssueRepository
import com.crossdashboard.app.domain.model.GiteaAttachment
import com.crossdashboard.app.domain.model.GiteaComment
import com.crossdashboard.app.domain.model.GiteaIssue
import dagger.hilt.android.lifecycle.HiltViewModel
import kotlinx.coroutines.async
import kotlinx.coroutines.flow.*
import kotlinx.coroutines.launch
import javax.inject.Inject

enum class IssueStateFilter { OPEN, CLOSED, ALL }

/** A file chosen by the user that has not yet been uploaded. */
data class PendingAttachment(
    val fileName: String,
    val mimeType: String,
    val bytes: ByteArray,
) {
    override fun equals(other: Any?) =
        other is PendingAttachment && fileName == other.fileName && mimeType == other.mimeType
    override fun hashCode(): Int = 31 * fileName.hashCode() + mimeType.hashCode()
}

data class IssuesUiState(
    val issues: List<GiteaIssue> = emptyList(),
    val filter: IssueStateFilter = IssueStateFilter.OPEN,
    val availableLabels: List<String> = emptyList(),
    val selectedLabel: String? = null,
    val isLoading: Boolean = false,
    val error: String? = null,
    /** Comments keyed by issue id — loaded lazily on sheet open */
    val comments: Map<Long, List<GiteaComment>> = emptyMap(),
    val commentLoading: Set<Long> = emptySet(),
    /** Attachments keyed by issue id */
    val issueAttachments: Map<Long, List<GiteaAttachment>> = emptyMap(),
    /** Attachments keyed by comment id */
    val commentAttachments: Map<Long, List<GiteaAttachment>> = emptyMap(),
    val showCreateSheet: Boolean = false,
    val isCreating: Boolean = false,
    val configuredRepos: List<String> = emptyList(),
)

@HiltViewModel
class IssuesViewModel @Inject constructor(
    private val issueRepo: IssueRepository,
    private val secureStore: SecureStore,
) : ViewModel() {

    private val _filter = MutableStateFlow(IssueStateFilter.OPEN)
    private val _selectedLabel = MutableStateFlow<String?>(null)
    private val _state = MutableStateFlow(IssuesUiState())
    val state: StateFlow<IssuesUiState> = _state.asStateFlow()

    init {
        val repos = secureStore.get(CredentialKey.GITEA_REPOS)
            ?.split(",")?.map { it.trim() }?.filter { it.isNotBlank() }
            ?: emptyList()
        _state.update { it.copy(configuredRepos = repos) }

        viewModelScope.launch {
            combine(issueRepo.allIssues, _filter, _selectedLabel) { issues, filter, selectedLabel ->
                val stateFiltered = when (filter) {
                    IssueStateFilter.OPEN -> issues.filter { it.state == "open" }
                    IssueStateFilter.CLOSED -> issues.filter { it.state == "closed" }
                    IssueStateFilter.ALL -> issues
                }
                Triple(
                    stateFiltered
                        .filter { selectedLabel == null || selectedLabel in it.labels }
                        .sortedByDescending { it.updatedAt },
                    issues.flatMap { it.labels }.distinct().sortedBy { it.lowercase() },
                    selectedLabel,
                )
            }.collect { (filtered, labels, selectedLabel) ->
                _state.update {
                    it.copy(
                        issues = filtered,
                        filter = _filter.value,
                        availableLabels = labels,
                        selectedLabel = selectedLabel,
                    )
                }
            }
        }
    }

    fun setFilter(filter: IssueStateFilter) {
        _filter.value = filter
    }

    fun setLabelFilter(label: String?) {
        _selectedLabel.value = label
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

    // ─── Create issue ─────────────────────────────────────────────────────────

    fun showCreateSheet() {
        val repos = secureStore.get(CredentialKey.GITEA_REPOS)
            ?.split(",")?.map { it.trim() }?.filter { it.isNotBlank() }
            ?: emptyList()
        _state.update { it.copy(showCreateSheet = true, configuredRepos = repos) }
    }

    fun dismissCreateSheet() = _state.update { it.copy(showCreateSheet = false) }

    fun createIssue(
        repo: String,
        title: String,
        body: String,
        attachments: List<PendingAttachment> = emptyList(),
    ) {
        if (title.isBlank() || repo.isBlank()) return
        viewModelScope.launch {
            _state.update { it.copy(isCreating = true, error = null) }
            try {
                val issue = issueRepo.createIssue(repo, title.trim(), body.trim())
                for (att in attachments) {
                    runCatching {
                        issueRepo.attachToIssue(repo, issue.number, att.fileName, att.bytes, att.mimeType)
                    }
                }
                // Pre-load attachments so they appear when the user opens the new issue
                if (attachments.isNotEmpty()) {
                    val uploaded = runCatching {
                        issueRepo.fetchIssueAttachments(repo, issue.number)
                    }.getOrDefault(emptyList())
                    if (uploaded.isNotEmpty()) {
                        _state.update {
                            it.copy(issueAttachments = it.issueAttachments + (issue.id to uploaded))
                        }
                    }
                }
                _state.update { it.copy(isCreating = false, showCreateSheet = false) }
            } catch (e: Exception) {
                _state.update { it.copy(isCreating = false, error = e.message) }
            }
        }
    }

    // ─── Comments ─────────────────────────────────────────────────────────────

    fun loadComments(issue: GiteaIssue) {
        if (issue.id in _state.value.comments) return  // already loaded
        viewModelScope.launch {
            _state.update { it.copy(commentLoading = it.commentLoading + issue.id) }
            try {
                val commentsDeferred = async { issueRepo.fetchComments(issue.repository, issue.number) }
                val issueAttachmentsDeferred = async {
                    issueRepo.fetchIssueAttachments(issue.repository, issue.number)
                }

                val comments = commentsDeferred.await()

                // Fetch per-comment attachments in parallel
                val commentAttachmentPairs = comments.map { comment ->
                    comment.id to async {
                        issueRepo.fetchCommentAttachments(issue.repository, comment.id)
                    }
                }.map { (id, deferred) -> id to deferred.await() }

                val newCommentAttachments = commentAttachmentPairs
                    .filter { (_, atts) -> atts.isNotEmpty() }
                    .toMap()

                _state.update {
                    it.copy(
                        comments = it.comments + (issue.id to comments),
                        issueAttachments = it.issueAttachments + (issue.id to issueAttachmentsDeferred.await()),
                        commentAttachments = it.commentAttachments + newCommentAttachments,
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

    fun addComment(
        issue: GiteaIssue,
        body: String,
        attachments: List<PendingAttachment> = emptyList(),
    ) {
        if (body.isBlank() && attachments.isEmpty()) return
        viewModelScope.launch {
            try {
                val comment = issueRepo.addComment(issue.repository, issue.number, body)
                for (att in attachments) {
                    runCatching {
                        issueRepo.attachToComment(issue.repository, comment.id, att.fileName, att.bytes, att.mimeType)
                    }
                }
                // Refresh comment attachments so newly uploaded files appear immediately
                val uploadedAtts = if (attachments.isNotEmpty()) {
                    runCatching { issueRepo.fetchCommentAttachments(issue.repository, comment.id) }
                        .getOrDefault(emptyList())
                } else emptyList()

                val existing = _state.value.comments[issue.id] ?: emptyList()
                _state.update {
                    it.copy(
                        comments = it.comments + (issue.id to existing + comment),
                        commentAttachments = if (uploadedAtts.isNotEmpty())
                            it.commentAttachments + (comment.id to uploadedAtts)
                        else it.commentAttachments,
                    )
                }
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

    fun saveIssue(issue: GiteaIssue, title: String, body: String, labels: List<String>) {
        viewModelScope.launch {
            try {
                issueRepo.update(issue.repository, issue.number, title = title, body = body)
                if (labels != issue.labels) {
                    issueRepo.replaceLabels(issue.repository, issue.number, labels)
                }
            } catch (e: Exception) {
                _state.update { it.copy(error = e.message) }
            }
        }
    }

    fun dismissError() = _state.update { it.copy(error = null) }
}
