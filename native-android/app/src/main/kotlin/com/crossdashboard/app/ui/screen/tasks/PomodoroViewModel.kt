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
import com.crossdashboard.app.service.PomodoroCommandBus
import com.crossdashboard.app.service.PomodoroForegroundService
import dagger.hilt.android.lifecycle.HiltViewModel
import dagger.hilt.android.qualifiers.ApplicationContext
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.first
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
 * Timer state is persisted to SharedPreferences so sessions survive Activity
 * destruction and process restarts. [PomodoroCommandBus] carries pause/resume/stop
 * commands from notification actions back into this ViewModel.
 *
 * On init the ViewModel loads settings from DataStore first, then attempts to
 * restore any previously active session from SharedPreferences. If the session
 * was running, the remaining time is computed from the persisted wall-clock
 * deadline rather than a stale [secondsLeft] snapshot.
 *
 * Scoped to the Activity so the same instance is shared across all composables
 * via [hiltViewModel] called at the root [CrossDashboardRoot] level.
 */
@HiltViewModel
class PomodoroViewModel @Inject constructor(
    @ApplicationContext private val context: Context,
    private val statsRepository: StatsRepository,
    private val appPreferences: AppPreferences,
    private val commandBus: PomodoroCommandBus,
) : ViewModel() {

    private val _state = MutableStateFlow(PomodoroUiState())
    val state: StateFlow<PomodoroUiState> = _state.asStateFlow()

    private var timer: CountDownTimer? = null
    private var onSessionComplete: (() -> Unit)? = null

    private val timerPrefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)

    init {
        viewModelScope.launch {
            // Load settings before restoring so phase durations are available
            val settings = appPreferences.pomodoroSettingsFlow.first()
            _state.update { it.copy(settings = settings) }
            restoreStateIfActive()
            // Continue collecting settings changes for the rest of the session
            appPreferences.pomodoroSettingsFlow.collect { newSettings ->
                _state.update { it.copy(settings = newSettings) }
            }
        }
        viewModelScope.launch {
            // Relay notification action commands from the foreground service
            commandBus.commands.collect { command ->
                when (command) {
                    PomodoroForegroundService.ACTION_PAUSE -> pause()
                    PomodoroForegroundService.ACTION_RESUME -> resume()
                    PomodoroForegroundService.ACTION_STOP -> stop()
                }
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
        persistRunning()
        startForegroundService()
        scheduleNextTick()
    }

    fun pause() {
        timer?.cancel()
        timer = null
        _state.update { it.copy(running = false) }
        persistPaused()
        sendServiceCommand(PomodoroForegroundService.ACTION_UPDATE)
    }

    fun resume() {
        if (!_state.value.active) return
        _state.update { it.copy(running = true) }
        persistRunning()
        scheduleNextTick()
        sendServiceCommand(PomodoroForegroundService.ACTION_UPDATE)
    }

    fun stop() {
        timer?.cancel()
        timer = null
        persistClear()
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
                // Throttle notification updates to limit binder traffic
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

        persistRunning()
        startForegroundService()
        scheduleNextTick()
    }

    // ─── State persistence ───────────────────────────────────────────────────

    /**
     * On startup, check SharedPreferences for an active session and restore
     * the ViewModel state. If the session was running, the remaining time is
     * derived from the stored wall-clock deadline rather than a frozen counter,
     * so even a process restart gives an accurate remaining time.
     */
    private fun restoreStateIfActive() {
        if (!timerPrefs.getBoolean(PREF_ACTIVE, false)) return

        val isRunning = timerPrefs.getBoolean(PREF_RUNNING, false)
        val phaseName = timerPrefs.getString(PREF_PHASE, PomodoroPhase.WORK.name) ?: PomodoroPhase.WORK.name
        val phase = runCatching { PomodoroPhase.valueOf(phaseName) }.getOrDefault(PomodoroPhase.WORK)
        val title = timerPrefs.getString(PREF_TITLE, "") ?: ""
        val completedSessions = timerPrefs.getInt(PREF_COMPLETED_SESSIONS, 0)
        val currentSession = timerPrefs.getInt(PREF_CURRENT_SESSION, 1)

        val secondsLeft: Int
        if (isRunning) {
            val endMs = timerPrefs.getLong(PREF_PHASE_END_MS, 0L)
            val remaining = ((endMs - System.currentTimeMillis()) / 1000).toInt()
            if (remaining <= 0) {
                // Phase ended while the app was away — clean up the stale session
                persistClear()
                context.startService(Intent(context, PomodoroForegroundService::class.java).apply {
                    action = PomodoroForegroundService.ACTION_STOP
                })
                return
            }
            secondsLeft = remaining
        } else {
            secondsLeft = timerPrefs.getInt(PREF_SECONDS_LEFT, _state.value.settings.workMinutes * 60)
        }

        _state.update {
            it.copy(
                active = true,
                running = isRunning,
                phase = phase,
                secondsLeft = secondsLeft,
                itemTitle = title,
                completedSessions = completedSessions,
                currentSession = currentSession,
            )
        }

        // Ensure the foreground service is alive (it may have been killed along with the process)
        startForegroundService()
        if (isRunning) scheduleNextTick()
    }

    /** Persist the running-timer state. Stores a wall-clock deadline so accuracy is maintained across process restarts. */
    private fun persistRunning() {
        val s = _state.value
        timerPrefs.edit()
            .putBoolean(PREF_ACTIVE, true)
            .putBoolean(PREF_RUNNING, true)
            .putString(PREF_PHASE, s.phase.name)
            .putString(PREF_TITLE, s.itemTitle)
            .putInt(PREF_COMPLETED_SESSIONS, s.completedSessions)
            .putInt(PREF_CURRENT_SESSION, s.currentSession)
            .putLong(PREF_PHASE_END_MS, System.currentTimeMillis() + s.secondsLeft * 1000L)
            .apply()
    }

    /** Persist the paused-timer state. Stores [secondsLeft] directly since the clock isn't advancing. */
    private fun persistPaused() {
        val s = _state.value
        timerPrefs.edit()
            .putBoolean(PREF_ACTIVE, true)
            .putBoolean(PREF_RUNNING, false)
            .putString(PREF_PHASE, s.phase.name)
            .putString(PREF_TITLE, s.itemTitle)
            .putInt(PREF_COMPLETED_SESSIONS, s.completedSessions)
            .putInt(PREF_CURRENT_SESSION, s.currentSession)
            .putInt(PREF_SECONDS_LEFT, s.secondsLeft)
            .apply()
    }

    private fun persistClear() {
        timerPrefs.edit().clear().apply()
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
                putExtra(PomodoroForegroundService.EXTRA_RUNNING, s.running)
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
                putExtra(PomodoroForegroundService.EXTRA_RUNNING, s.running)
            }
        )
    }

    override fun onCleared() {
        super.onCleared()
        timer?.cancel()
    }

    companion object {
        private const val PREFS_NAME = "pomodoro_timer_state"
        private const val PREF_ACTIVE = "active"
        private const val PREF_RUNNING = "running"
        private const val PREF_PHASE = "phase"
        private const val PREF_TITLE = "title"
        private const val PREF_COMPLETED_SESSIONS = "completed_sessions"
        private const val PREF_CURRENT_SESSION = "current_session"
        private const val PREF_PHASE_END_MS = "phase_end_ms"
        private const val PREF_SECONDS_LEFT = "seconds_left"
    }
}
