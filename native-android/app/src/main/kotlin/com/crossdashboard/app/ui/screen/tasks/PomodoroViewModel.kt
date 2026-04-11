package com.crossdashboard.app.ui.screen.tasks

import android.content.Context
import android.content.Intent
import android.os.CountDownTimer
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.crossdashboard.app.data.prefs.AppPreferences
import com.crossdashboard.app.data.repository.StatsRepository
import com.crossdashboard.app.domain.model.PomodoroPhase
import com.crossdashboard.app.domain.model.PomodoroSettings
import com.crossdashboard.app.service.PomodoroForegroundService
import dagger.hilt.android.lifecycle.HiltViewModel
import dagger.hilt.android.qualifiers.ApplicationContext
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import javax.inject.Inject

/**
 * UI state for the Pomodoro overlay. Separate from the domain PomodoroState
 * to carry UI-only fields like [modalVisible].
 */
data class PomodoroUiState(
    val phase: PomodoroPhase = PomodoroPhase.WORK,
    val secondsLeft: Int = 25 * 60,
    val running: Boolean = false,
    val currentSession: Int = 1,
    val completedSessions: Int = 0,
    val itemTitle: String = "",
    val active: Boolean = false,
    val settings: PomodoroSettings = PomodoroSettings(),
    val modalVisible: Boolean = false,
)

/**
 * Drives the Pomodoro timer, foreground service notifications, and session stats.
 *
 * Scoped to the Activity so the same instance is shared across all composables
 * via [hiltViewModel] called at the root [CrossDashboardRoot] level.
 */
@HiltViewModel
class PomodoroViewModel @Inject constructor(
    @ApplicationContext private val context: Context,
    private val statsRepository: StatsRepository,
    private val appPreferences: AppPreferences,
) : ViewModel() {

    private val _state = MutableStateFlow(PomodoroUiState())
    val state: StateFlow<PomodoroUiState> = _state.asStateFlow()

    private var timer: CountDownTimer? = null
    private var onSessionComplete: (() -> Unit)? = null

    init {
        viewModelScope.launch {
            appPreferences.pomodoroSettingsFlow.collect { settings ->
                _state.update { it.copy(settings = settings) }
            }
        }
    }

    // ─── Public API ──────────────────────────────────────────────────────────

    fun start(title: String, onComplete: (() -> Unit)? = null) {
        onSessionComplete = onComplete
        val settings = _state.value.settings
        _state.update {
            it.copy(
                active = true,
                running = true,
                itemTitle = title,
                phase = PomodoroPhase.WORK,
                secondsLeft = settings.workMinutes * 60,
                currentSession = 1,
                completedSessions = 0,
                modalVisible = false,
            )
        }
        startForegroundService()
        scheduleNextTick()
    }

    fun pause() {
        timer?.cancel()
        timer = null
        _state.update { it.copy(running = false) }
        sendServiceCommand(PomodoroForegroundService.ACTION_UPDATE)
    }

    fun resume() {
        if (!_state.value.active) return
        _state.update { it.copy(running = true) }
        scheduleNextTick()
        sendServiceCommand(PomodoroForegroundService.ACTION_UPDATE)
    }

    fun stop() {
        timer?.cancel()
        timer = null
        _state.update { PomodoroUiState() }
        context.startService(
            Intent(context, PomodoroForegroundService::class.java).apply {
                action = PomodoroForegroundService.ACTION_STOP
            }
        )
    }

    fun showModal() = _state.update { it.copy(modalVisible = true) }
    fun hideModal() = _state.update { it.copy(modalVisible = false) }

    fun skipPhase() {
        timer?.cancel()
        timer = null
        onPhaseEnd()
    }

    // ─── Timer internals ─────────────────────────────────────────────────────

    private fun scheduleNextTick() {
        val secondsLeft = _state.value.secondsLeft
        timer?.cancel()
        timer = object : CountDownTimer(secondsLeft * 1000L, 1000L) {
            override fun onTick(millisUntilFinished: Long) {
                val secs = (millisUntilFinished / 1000).toInt()
                _state.update { it.copy(secondsLeft = secs) }
                // Update notification every 5 seconds to limit binder calls
                if (secs % 5 == 0) sendServiceCommand(PomodoroForegroundService.ACTION_UPDATE)
            }

            override fun onFinish() {
                _state.update { it.copy(secondsLeft = 0) }
                onPhaseEnd()
            }
        }.start()
    }

    private fun onPhaseEnd() {
        val current = _state.value
        val settings = current.settings

        when (current.phase) {
            PomodoroPhase.WORK -> {
                val newCompleted = current.completedSessions + 1
                viewModelScope.launch {
                    statsRepository.incrementPomodoro()
                    onSessionComplete?.invoke()
                }
                val nextPhase = if (newCompleted % settings.sessionsUntilLongBreak == 0) {
                    PomodoroPhase.LONG_BREAK
                } else {
                    PomodoroPhase.SHORT_BREAK
                }
                val nextSeconds = when (nextPhase) {
                    PomodoroPhase.SHORT_BREAK -> settings.shortBreakMinutes * 60
                    PomodoroPhase.LONG_BREAK -> settings.longBreakMinutes * 60
                    PomodoroPhase.WORK -> settings.workMinutes * 60
                }
                _state.update {
                    it.copy(
                        phase = nextPhase,
                        secondsLeft = nextSeconds,
                        completedSessions = newCompleted,
                        running = true,
                    )
                }
            }
            PomodoroPhase.SHORT_BREAK, PomodoroPhase.LONG_BREAK -> {
                val nextSeconds = settings.workMinutes * 60
                _state.update {
                    it.copy(
                        phase = PomodoroPhase.WORK,
                        secondsLeft = nextSeconds,
                        currentSession = it.currentSession + 1,
                        running = true,
                    )
                }
            }
        }

        startForegroundService()
        scheduleNextTick()
    }

    // ─── Service communication ───────────────────────────────────────────────

    private fun startForegroundService() {
        val s = _state.value
        context.startForegroundService(
            Intent(context, PomodoroForegroundService::class.java).apply {
                action = PomodoroForegroundService.ACTION_START
                putExtra(PomodoroForegroundService.EXTRA_TITLE, s.itemTitle)
                putExtra(PomodoroForegroundService.EXTRA_PHASE, s.phase.name)
                putExtra(PomodoroForegroundService.EXTRA_SECONDS_LEFT, s.secondsLeft)
            }
        )
    }

    private fun sendServiceCommand(action: String) {
        val s = _state.value
        if (!s.active) return
        context.startService(
            Intent(context, PomodoroForegroundService::class.java).apply {
                this.action = action
                putExtra(PomodoroForegroundService.EXTRA_TITLE, s.itemTitle)
                putExtra(PomodoroForegroundService.EXTRA_PHASE, s.phase.name)
                putExtra(PomodoroForegroundService.EXTRA_SECONDS_LEFT, s.secondsLeft)
            }
        )
    }

    override fun onCleared() {
        super.onCleared()
        timer?.cancel()
    }
}
