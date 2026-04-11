package com.crossdashboard.app.alarm

import android.app.AlarmManager
import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import com.crossdashboard.app.domain.model.CalDavTask
import com.crossdashboard.app.domain.model.TaskStatus
import dagger.hilt.android.qualifiers.ApplicationContext
import java.time.Instant
import javax.inject.Inject
import javax.inject.Singleton

/**
 * Schedules and cancels exact due-time reminder alarms for CalDAV tasks.
 *
 * Only root tasks (no parentUid) with a future due date and non-terminal status
 * receive an alarm. A single alarm fires [minutesBefore] minutes before the due
 * time; the value comes from [AppPreferences.notificationsFlow] and is read by
 * [SyncWorker] before calling [rescheduleAll].
 *
 * Alarm IDs occupy the range 200,000–299,999 to avoid collisions with the event
 * alarm ranges (0–199,999) used by [EventAlarmScheduler].
 */
@Singleton
class TaskAlarmScheduler @Inject constructor(
    @ApplicationContext private val context: Context,
) {
    private val alarmManager = context.getSystemService(Context.ALARM_SERVICE) as AlarmManager

    /**
     * Cancel all existing task alarms then schedule fresh ones for all eligible
     * [tasks]. Called by [SyncWorker] after every successful task sync.
     */
    fun rescheduleAll(tasks: List<CalDavTask>, minutesBefore: Int) {
        tasks.forEach { cancel(it.uid) }

        val now = Instant.now()
        tasks
            .filter { task ->
                task.parentUid == null &&
                    task.due != null &&
                    task.due.isAfter(now) &&
                    task.status != TaskStatus.COMPLETED &&
                    task.status != TaskStatus.CANCELLED
            }
            .forEach { schedule(it, minutesBefore) }
    }

    private fun schedule(task: CalDavTask, minutesBefore: Int) {
        val dueMs = task.due!!.toEpochMilli()
        val triggerMs = dueMs - minutesBefore * 60_000L
        if (triggerMs <= System.currentTimeMillis()) return

        val alarmId = Math.abs(task.uid.hashCode()) % 100_000 + ALARM_ID_OFFSET
        val pi = buildPendingIntent(alarmId, task.uid, task.summary, minutesBefore)
        runCatching {
            alarmManager.setExactAndAllowWhileIdle(AlarmManager.RTC_WAKEUP, triggerMs, pi)
        }
    }

    fun cancel(uid: String) {
        val alarmId = Math.abs(uid.hashCode()) % 100_000 + ALARM_ID_OFFSET
        val pi = buildPendingIntent(alarmId, uid, "", 0)
        alarmManager.cancel(pi)
    }

    private fun buildPendingIntent(
        alarmId: Int,
        uid: String,
        summary: String,
        minutesBefore: Int,
    ): PendingIntent {
        val intent = Intent(context, TaskAlarmReceiver::class.java).apply {
            action = TaskAlarmReceiver.ACTION_ALARM
            putExtra(TaskAlarmReceiver.EXTRA_UID, uid)
            putExtra(TaskAlarmReceiver.EXTRA_SUMMARY, summary)
            putExtra(TaskAlarmReceiver.EXTRA_MINUTES_BEFORE, minutesBefore)
            putExtra(TaskAlarmReceiver.EXTRA_NOTIF_ID, alarmId)
        }
        return PendingIntent.getBroadcast(
            context,
            alarmId,
            intent,
            PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT,
        )
    }

    companion object {
        private const val ALARM_ID_OFFSET = 200_000
    }
}
