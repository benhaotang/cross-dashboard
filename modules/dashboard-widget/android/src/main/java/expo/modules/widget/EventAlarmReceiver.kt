package expo.modules.widget

import android.Manifest
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.os.Build
import androidx.core.app.NotificationCompat
import androidx.core.content.ContextCompat

class EventAlarmReceiver : BroadcastReceiver() {

    companion object {
        const val CHANNEL_ID = "event_reminders"
        const val EXTRA_SUMMARY = "summary"
        const val EXTRA_LOCATION = "location"
        const val EXTRA_TYPE = "type"
        const val EXTRA_MINUTES = "minutes"
        const val EXTRA_NOTIF_ID = "notif_id"

        fun createChannel(context: Context) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                val manager = context.getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
                if (manager.getNotificationChannel(CHANNEL_ID) == null) {
                    val channel = NotificationChannel(
                        CHANNEL_ID,
                        "Event Reminders",
                        NotificationManager.IMPORTANCE_HIGH
                    ).apply {
                        description = "Reminders for upcoming calendar events"
                    }
                    manager.createNotificationChannel(channel)
                }
            }
        }
    }

    override fun onReceive(context: Context, intent: Intent) {
        // Check POST_NOTIFICATIONS permission on API 33+
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            if (ContextCompat.checkSelfPermission(
                    context, Manifest.permission.POST_NOTIFICATIONS
                ) != PackageManager.PERMISSION_GRANTED
            ) return
        }

        val summary = intent.getStringExtra(EXTRA_SUMMARY) ?: return
        val location = intent.getStringExtra(EXTRA_LOCATION)
        val type = intent.getStringExtra(EXTRA_TYPE) ?: "at_time"
        val minutes = intent.getIntExtra(EXTRA_MINUTES, 15)
        val notifId = intent.getIntExtra(EXTRA_NOTIF_ID, 0)

        val body = when (type) {
            "remind_before" -> "In $minutes minutes" + if (location != null) " at $location" else ""
            else -> "Starting now" + if (location != null) " at $location" else ""
        }

        createChannel(context)

        val launchIntent = context.packageManager.getLaunchIntentForPackage(context.packageName)
            ?: Intent()
        launchIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_SINGLE_TOP)
        val pendingIntent = PendingIntent.getActivity(
            context, notifId, launchIntent,
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
        )

        val notification = NotificationCompat.Builder(context, CHANNEL_ID)
            .setSmallIcon(android.R.drawable.ic_dialog_info)
            .setContentTitle(summary)
            .setContentText(body)
            .setPriority(NotificationCompat.PRIORITY_HIGH)
            .setContentIntent(pendingIntent)
            .setAutoCancel(true)
            .build()

        val manager = context.getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
        manager.notify(notifId, notification)
    }
}
