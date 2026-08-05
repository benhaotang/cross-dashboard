package com.crossdashboard.app.ui.screen.events

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.crossdashboard.app.data.prefs.CredentialKey
import com.crossdashboard.app.data.prefs.SecureStore
import com.crossdashboard.app.data.repository.EventRepository
import com.crossdashboard.app.domain.model.CalDavCalendar
import com.crossdashboard.app.domain.model.CalendarEvent
import dagger.hilt.android.lifecycle.HiltViewModel
import kotlinx.coroutines.flow.*
import kotlinx.coroutines.launch
import kotlinx.serialization.json.Json
import java.time.Instant
import java.time.temporal.ChronoUnit
import javax.inject.Inject

enum class EventFilter { DAY, WEEK, MONTH }

data class EventsUiState(
    val allEvents: List<CalendarEvent> = emptyList(),
    val events: List<CalendarEvent> = emptyList(),
    val filter: EventFilter = EventFilter.WEEK,
    val isLoading: Boolean = false,
    val error: String? = null,
)

@HiltViewModel
class EventsViewModel @Inject constructor(
    private val eventRepo: EventRepository,
    private val secureStore: SecureStore,
) : ViewModel() {

    private val _filter = MutableStateFlow(EventFilter.WEEK)
    private val _state = MutableStateFlow(EventsUiState())
    val state: StateFlow<EventsUiState> = _state.asStateFlow()

    init {
        viewModelScope.launch {
            combine(eventRepo.events, _filter) { events, filter ->
                val now = Instant.now()
                val cutoff = when (filter) {
                    EventFilter.DAY -> now.plus(1, ChronoUnit.DAYS)
                    EventFilter.WEEK -> now.plus(7, ChronoUnit.DAYS)
                    EventFilter.MONTH -> now.plus(31, ChronoUnit.DAYS)
                }
                val startCutoff = when (filter) {
                    EventFilter.DAY -> now.minus(1, ChronoUnit.DAYS)
                    EventFilter.WEEK -> now.minus(7, ChronoUnit.DAYS)
                    EventFilter.MONTH -> now.minus(31, ChronoUnit.DAYS)
                }
                val filtered = events
                    .filter { it.start.isAfter(startCutoff) && it.start.isBefore(cutoff) }
                    .sortedBy { it.start }
                Triple(events, filtered, filter)
            }.collect { (allEvents, filtered, filter) ->
                _state.update {
                    it.copy(allEvents = allEvents, events = filtered, filter = filter)
                }
            }
        }
    }

    fun setFilter(filter: EventFilter) {
        _filter.value = filter
    }

    fun sync() {
        viewModelScope.launch {
            _state.update { it.copy(isLoading = true, error = null) }
            try {
                eventRepo.sync(selectedCalendarHrefs())
            } catch (e: Exception) {
                _state.update { it.copy(error = e.message) }
            } finally {
                _state.update { it.copy(isLoading = false) }
            }
        }
    }

    private fun selectedCalendarHrefs(): List<String> {
        val raw = secureStore.get(CredentialKey.CALDAV_SELECTED_CALENDARS) ?: return emptyList()
        return runCatching {
            Json.decodeFromString<List<CalDavCalendar>>(raw).map { it.href }
        }.getOrDefault(emptyList())
    }

    fun dismissError() = _state.update { it.copy(error = null) }
}
