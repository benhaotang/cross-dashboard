package com.crossdashboard.app.alarm

import android.app.AlarmManager
import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import com.crossdashboard.app.domain.model.CalendarEvent
import dagger.hilt.android.qualifiers.ApplicationContext
import java.time.Instant
import javax.inject.Inject
import javax.inject.Singleton

/**
 * Schedules and cancels exact alarms for CalDAV events using [AlarmManager].
 *
 * Uses [android.Manifest.permission.USE_EXACT_ALARM] (auto-granted for calendar apps,
 * declared in AndroidManifest). We do NOT declare SCHEDULE_EXACT_ALARM to avoid
 * the user-revocable permission lifecycle.
 *
 * Alarm ID strategy (stable, collision-free within 200k range):
 *   - at-time alarm:     abs(uid.hashCode()) % 100_000
 *   - remind-before:     abs(uid.hashCode()) % 100_000 + 100_000
 */
@Singleton
class EventAlarmScheduler @Inject constructor(
    @ApplicationContext private val context: Context,
) {
    private val alarmManager = context.getSystemService(Context.ALARM_SERVICE) as AlarmManager

    /**
     * Cancel all old alarms then schedule fresh ones for all future [events].
     * Called after every sync and after boot.
     */
    fun rescheduleAll(events: List<CalendarEvent>) {
        // Cancel everything first (idempotent, safe to call on stale IDs)
        events.forEach { cancel(it.uid) }

        val now = Instant.now()
        events
            .filter { it.start.isAfter(now) }
            .forEach { schedule(it) }
    }

    private fun schedule(event: CalendarEvent) {
        val baseId = Math.abs(event.uid.hashCode()) % 100_000
        val startMs = event.start.toEpochMilli()
        val remindMs = startMs - REMIND_BEFORE_MINUTES * 60_000L
        val now = System.currentTimeMillis()

        // At-time alarm
        if (startMs > now) {
            setAlarm(
                alarmId = baseId,
                triggerMs = startMs,
                uid = event.uid,
                summary = event.summary,
                location = event.location,
                type = TYPE_AT_TIME,
                minutesBefore = 0,
            )
        }

        // Remind-before alarm
        if (remindMs > now) {
            setAlarm(
                alarmId = baseId + 100_000,
                triggerMs = remindMs,
                uid = event.uid,
                summary = event.summary,
                location = event.location,
                type = TYPE_REMIND_BEFORE,
                minutesBefore = REMIND_BEFORE_MINUTES,
            )
        }
    }

    private fun setAlarm(
        alarmId: Int,
        triggerMs: Long,
        uid: String,
        summary: String,
        location: String?,
        type: String,
        minutesBefore: Int,
    ) {
        val pi = buildPendingIntent(alarmId, uid, summary, location, type, minutesBefore)
        runCatching {
            alarmManager.setExactAndAllowWhileIdle(AlarmManager.RTC_WAKEUP, triggerMs, pi)
        }
    }

    fun cancel(uid: String) {
        val baseId = Math.abs(uid.hashCode()) % 100_000
        listOf(baseId, baseId + 100_000).forEach { alarmId ->
            // Extras are not used for PendingIntent matching — empty values are safe here
            val pi = buildPendingIntent(alarmId, uid, "", null, TYPE_AT_TIME, 0)
            alarmManager.cancel(pi)
        }
    }

    private fun buildPendingIntent(
        alarmId: Int,
        uid: String,
        summary: String,
        location: String?,
        type: String,
        minutesBefore: Int,
    ): PendingIntent {
        val intent = Intent(context, EventAlarmReceiver::class.java).apply {
            action = EventAlarmReceiver.ACTION_ALARM
            putExtra(EventAlarmReceiver.EXTRA_UID, uid)
            putExtra(EventAlarmReceiver.EXTRA_SUMMARY, summary)
            putExtra(EventAlarmReceiver.EXTRA_LOCATION, location)
            putExtra(EventAlarmReceiver.EXTRA_TYPE, type)
            putExtra(EventAlarmReceiver.EXTRA_MINUTES_BEFORE, minutesBefore)
            putExtra(EventAlarmReceiver.EXTRA_NOTIF_ID, alarmId)
        }
        return PendingIntent.getBroadcast(
            context,
            alarmId,
            intent,
            PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT,
        )
    }

    companion object {
        const val REMIND_BEFORE_MINUTES = 15
        const val TYPE_AT_TIME = "at_time"
        const val TYPE_REMIND_BEFORE = "remind_before"
    }
}
