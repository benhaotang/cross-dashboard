package com.crossdashboard.app.alarm

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import androidx.core.app.NotificationCompat
import androidx.core.app.NotificationManagerCompat
import androidx.core.content.ContextCompat
import com.crossdashboard.app.CrossDashboardApp.Companion.CHANNEL_EVENTS
import com.crossdashboard.app.R

/**
 * Fires an event reminder notification when an exact alarm triggers.
 *
 * Alarm types:
 * - [TYPE_AT_TIME]: "Starting now [at location]"
 * - [TYPE_REMIND_BEFORE]: "In N minutes [at location]"
 */
class EventAlarmReceiver : BroadcastReceiver() {

    override fun onReceive(context: Context, intent: Intent) {
        if (intent.action != ACTION_ALARM) return

        val summary = intent.getStringExtra(EXTRA_SUMMARY) ?: return
        val location = intent.getStringExtra(EXTRA_LOCATION)
        val type = intent.getStringExtra(EXTRA_TYPE) ?: TYPE_AT_TIME
        val minutesBefore = intent.getIntExtra(EXTRA_MINUTES_BEFORE, 0)
        val notifId = intent.getIntExtra(EXTRA_NOTIF_ID, System.currentTimeMillis().toInt())

        // Check POST_NOTIFICATIONS permission (required API 33+, minSdk=36)
        if (ContextCompat.checkSelfPermission(context, android.Manifest.permission.POST_NOTIFICATIONS)
            != PackageManager.PERMISSION_GRANTED
        ) return

        val bodyText = buildBody(type, minutesBefore, location)

        val notification = NotificationCompat.Builder(context, CHANNEL_EVENTS)
            .setSmallIcon(R.drawable.ic_notification)
            .setContentTitle(summary)
            .setContentText(bodyText)
            .setStyle(NotificationCompat.BigTextStyle().bigText(bodyText))
            .setPriority(NotificationCompat.PRIORITY_HIGH)
            .setAutoCancel(true)
            .build()

        NotificationManagerCompat.from(context).notify(notifId, notification)
    }

    private fun buildBody(type: String, minutesBefore: Int, location: String?): String {
        val locationSuffix = if (!location.isNullOrBlank()) " at $location" else ""
        return when (type) {
            TYPE_REMIND_BEFORE -> "In $minutesBefore minutes$locationSuffix"
            else -> "Starting now$locationSuffix"
        }
    }

    companion object {
        const val ACTION_ALARM = "com.crossdashboard.app.EVENT_ALARM"
        const val EXTRA_SUMMARY = "summary"
        const val EXTRA_LOCATION = "location"
        const val EXTRA_TYPE = "type"
        const val EXTRA_MINUTES_BEFORE = "minutes_before"
        const val EXTRA_NOTIF_ID = "notif_id"
        const val TYPE_AT_TIME = "at_time"
        const val TYPE_REMIND_BEFORE = "remind_before"
    }
}
