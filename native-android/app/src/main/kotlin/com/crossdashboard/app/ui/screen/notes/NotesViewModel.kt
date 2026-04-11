package com.crossdashboard.app.ui.screen.notes

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.crossdashboard.app.data.prefs.CredentialKey
import com.crossdashboard.app.data.prefs.SecureStore
import com.crossdashboard.app.data.repository.NoteRepository
import com.crossdashboard.app.domain.model.CalDavCalendar
import com.crossdashboard.app.domain.model.Note
import kotlinx.serialization.json.Json
import dagger.hilt.android.lifecycle.HiltViewModel
import kotlinx.coroutines.flow.*
import kotlinx.coroutines.launch
import java.time.Instant
import java.util.UUID
import javax.inject.Inject

data class NotesUiState(
    val notes: List<Note> = emptyList(),
    val searchQuery: String = "",
    val isLoading: Boolean = false,
    val error: String? = null,
    val defaultCalendarHref: String? = null,
)

@HiltViewModel
class NotesViewModel @Inject constructor(
    private val noteRepo: NoteRepository,
    private val secureStore: SecureStore,
) : ViewModel() {

    private val _search = MutableStateFlow("")
    private val _state = MutableStateFlow(NotesUiState())
    val state: StateFlow<NotesUiState> = _state.asStateFlow()

    init {
        viewModelScope.launch {
            combine(noteRepo.notes, _search) { notes, query ->
                if (query.isBlank()) notes
                else notes.filter {
                    it.summary.contains(query, ignoreCase = true) ||
                        it.body.contains(query, ignoreCase = true)
                }
            }.collect { filtered ->
                _state.update { it.copy(notes = filtered, searchQuery = _search.value) }
            }
        }

        viewModelScope.launch {
            _state.update {
                it.copy(defaultCalendarHref = secureStore.get(CredentialKey.CALDAV_DEFAULT_EVENT_CALENDAR))
            }
        }
    }

    fun onSearchChange(query: String) {
        _search.value = query
    }

    fun createNote(summary: String, body: String, categories: List<String>) {
        val calendarHref = _state.value.defaultCalendarHref ?: return
        viewModelScope.launch {
            _state.update { it.copy(isLoading = true, error = null) }
            try {
                val note = Note(
                    uid = UUID.randomUUID().toString(),
                    summary = summary,
                    body = body,
                    categories = categories,
                    created = Instant.now(),
                    lastModified = Instant.now(),
                )
                noteRepo.create(note, calendarHref)
            } catch (e: Exception) {
                _state.update { it.copy(error = e.message) }
            } finally {
                _state.update { it.copy(isLoading = false) }
            }
        }
    }

    fun updateNote(note: Note) {
        viewModelScope.launch {
            try {
                noteRepo.update(note)
            } catch (e: Exception) {
                _state.update { it.copy(error = e.message) }
            }
        }
    }

    fun deleteNote(note: Note) {
        viewModelScope.launch {
            try {
                noteRepo.delete(note)
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
                noteRepo.sync(hrefs)
            } catch (e: Exception) {
                _state.update { it.copy(error = e.message) }
            } finally {
                _state.update { it.copy(isLoading = false) }
            }
        }
    }

    fun dismissError() = _state.update { it.copy(error = null) }
}
