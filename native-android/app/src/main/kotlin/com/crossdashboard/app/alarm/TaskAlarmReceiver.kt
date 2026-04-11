package com.crossdashboard.app.alarm

import android.app.PendingIntent
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.net.Uri
import androidx.core.app.NotificationCompat
import androidx.core.app.NotificationManagerCompat
import androidx.core.content.ContextCompat
import com.crossdashboard.app.CrossDashboardApp.Companion.CHANNEL_TASKS
import com.crossdashboard.app.MainActivity
import com.crossdashboard.app.R

/**
 * Fires a task due-time reminder notification when an exact alarm triggers.
 *
 * The alarm is scheduled by [TaskAlarmScheduler] a configurable number of
 * minutes before the task's due time. Tapping the notification deep-links to
 * `crossdashboard://tasks?uid={uid}`, which [MainActivity] routes to the task
 * detail pane via [Destination.TaskDetail].
 */
class TaskAlarmReceiver : BroadcastReceiver() {

    override fun onReceive(context: Context, intent: Intent) {
        if (intent.action != ACTION_ALARM) return

        val uid = intent.getStringExtra(EXTRA_UID) ?: ""
        val summary = intent.getStringExtra(EXTRA_SUMMARY) ?: return
        val minutesBefore = intent.getIntExtra(EXTRA_MINUTES_BEFORE, 0)
        val notifId = intent.getIntExtra(EXTRA_NOTIF_ID, System.currentTimeMillis().toInt())

        if (ContextCompat.checkSelfPermission(context, android.Manifest.permission.POST_NOTIFICATIONS)
            != PackageManager.PERMISSION_GRANTED
        ) return

        val bodyText = if (minutesBefore > 0) {
            context.getString(R.string.task_reminder_due_in, minutesBefore)
        } else {
            context.getString(R.string.task_reminder_due_now)
        }

        val contentIntent = PendingIntent.getActivity(
            context,
            notifId,
            Intent(context, MainActivity::class.java).apply {
                action = Intent.ACTION_VIEW
                data = Uri.parse("crossdashboard://tasks?uid=${Uri.encode(uid)}")
                flags = Intent.FLAG_ACTIVITY_SINGLE_TOP
            },
            PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT,
        )

        val notification = NotificationCompat.Builder(context, CHANNEL_TASKS)
            .setSmallIcon(R.drawable.ic_notification)
            .setContentTitle(summary)
            .setContentText(bodyText)
            .setStyle(NotificationCompat.BigTextStyle().bigText(bodyText))
            .setPriority(NotificationCompat.PRIORITY_HIGH)
            .setAutoCancel(true)
            .setContentIntent(contentIntent)
            .build()

        NotificationManagerCompat.from(context).notify(notifId, notification)
    }

    companion object {
        const val ACTION_ALARM = "com.crossdashboard.app.TASK_ALARM"
        const val EXTRA_UID = "uid"
        const val EXTRA_SUMMARY = "summary"
        const val EXTRA_MINUTES_BEFORE = "minutes_before"
        const val EXTRA_NOTIF_ID = "notif_id"
    }
}
