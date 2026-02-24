package expo.modules.pomodoroservice

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent

/**
 * BroadcastReceiver triggered by AlarmManager exact alarms as a backup
 * mechanism to ensure phase transitions fire even if the app process
 * has been hibernated by the OS.
 */
class PomodoroAlarmReceiver : BroadcastReceiver() {
    override fun onReceive(context: Context, intent: Intent) {
        val phase = intent.getStringExtra(PomodoroForegroundService.EXTRA_PHASE) ?: "work"
        PomodoroEventBus.emit("onAlarmFired", mapOf("phase" to phase))
        PomodoroEventBus.emit("onPhaseEnd", mapOf("phase" to phase))
    }
}
