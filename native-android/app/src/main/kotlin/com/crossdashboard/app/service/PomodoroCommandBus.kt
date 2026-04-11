package com.crossdashboard.app.service

import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.flow.SharedFlow
import kotlinx.coroutines.flow.asSharedFlow
import javax.inject.Inject
import javax.inject.Singleton

/**
 * Singleton event bus that carries Pomodoro control commands from
 * [PomodoroForegroundService] notification actions back to [PomodoroViewModel].
 *
 * Using a Hilt singleton avoids LocalBroadcastManager (deprecated) and keeps
 * the command flow entirely within the process.
 */
@Singleton
class PomodoroCommandBus @Inject constructor() {
    private val _commands = MutableSharedFlow<String>(extraBufferCapacity = 1)
    val commands: SharedFlow<String> = _commands.asSharedFlow()

    fun send(command: String) {
        _commands.tryEmit(command)
    }
}
