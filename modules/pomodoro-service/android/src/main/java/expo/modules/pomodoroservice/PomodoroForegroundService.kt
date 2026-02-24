package expo.modules.pomodoroservice

import android.app.AlarmManager
import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.Context
import android.content.Intent
import android.os.Build
import android.os.CountDownTimer
import android.os.IBinder
import android.os.SystemClock
import android.widget.RemoteViews
import androidx.core.app.NotificationCompat

class PomodoroForegroundService : Service() {

    companion object {
        const val ACTION_START = "expo.modules.pomodoroservice.ACTION_START"
        const val ACTION_PAUSE = "expo.modules.pomodoroservice.ACTION_PAUSE"
        const val ACTION_RESUME = "expo.modules.pomodoroservice.ACTION_RESUME"
        const val ACTION_STOP = "expo.modules.pomodoroservice.ACTION_STOP"

        const val EXTRA_SECONDS = "seconds"
        const val EXTRA_TASK_NAME = "taskName"
        const val EXTRA_PHASE = "phase"

        private const val NOTIFICATION_ID = 7700
        private const val CHANNEL_ID = "pomodoro_timer"
        private const val ALARM_REQUEST_CODE = 7701
    }

    private var countDownTimer: CountDownTimer? = null
    private var secondsLeft: Int = 0
    private var taskName: String = ""
    private var phase: String = "work"
    private var isPaused: Boolean = false

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onCreate() {
        super.onCreate()
        createNotificationChannel()
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        when (intent?.action) {
            ACTION_START -> {
                secondsLeft = intent.getIntExtra(EXTRA_SECONDS, 25 * 60)
                val name = intent.getStringExtra(EXTRA_TASK_NAME) ?: ""
                if (name.isNotEmpty()) taskName = name
                phase = intent.getStringExtra(EXTRA_PHASE) ?: "work"
                isPaused = false
                startForeground(NOTIFICATION_ID, buildNotification())
                startCountdown()
                scheduleAlarm()
            }
            ACTION_PAUSE -> {
                isPaused = true
                countDownTimer?.cancel()
                countDownTimer = null
                cancelAlarm()
                updateNotification()
                PomodoroEventBus.emit("onAction", mapOf("action" to "pause"))
            }
            ACTION_RESUME -> {
                isPaused = false
                startCountdown()
                scheduleAlarm()
                PomodoroEventBus.emit("onAction", mapOf("action" to "resume"))
            }
            ACTION_STOP -> {
                countDownTimer?.cancel()
                countDownTimer = null
                cancelAlarm()
                PomodoroEventBus.emit("onAction", mapOf("action" to "stop"))
                stopForeground(STOP_FOREGROUND_REMOVE)
                stopSelf()
            }
        }
        return START_NOT_STICKY
    }

    override fun onDestroy() {
        countDownTimer?.cancel()
        cancelAlarm()
        super.onDestroy()
    }

    private fun startCountdown() {
        countDownTimer?.cancel()
        countDownTimer = object : CountDownTimer(secondsLeft * 1000L, 1000L) {
            override fun onTick(millisUntilFinished: Long) {
                secondsLeft = (millisUntilFinished / 1000).toInt()
                updateNotification()
                PomodoroEventBus.emit("onTick", mapOf("secondsLeft" to secondsLeft))
            }

            override fun onFinish() {
                secondsLeft = 0
                cancelAlarm()
                PomodoroEventBus.emit("onPhaseEnd", mapOf("phase" to phase))
                updateNotification()
            }
        }.start()
    }

    private fun scheduleAlarm() {
        if (secondsLeft <= 0) return
        val alarmManager = getSystemService(Context.ALARM_SERVICE) as AlarmManager
        val intent = Intent(this, PomodoroAlarmReceiver::class.java).apply {
            putExtra(EXTRA_PHASE, phase)
        }
        val pendingIntent = PendingIntent.getBroadcast(
            this, ALARM_REQUEST_CODE, intent,
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
        )
        val triggerAt = SystemClock.elapsedRealtime() + secondsLeft * 1000L
        try {
            alarmManager.setExactAndAllowWhileIdle(
                AlarmManager.ELAPSED_REALTIME_WAKEUP,
                triggerAt,
                pendingIntent
            )
        } catch (_: SecurityException) {
            // Exact alarm permission not granted; countdown timer is the fallback
        }
    }

    private fun cancelAlarm() {
        val alarmManager = getSystemService(Context.ALARM_SERVICE) as AlarmManager
        val intent = Intent(this, PomodoroAlarmReceiver::class.java)
        val pendingIntent = PendingIntent.getBroadcast(
            this, ALARM_REQUEST_CODE, intent,
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
        )
        alarmManager.cancel(pendingIntent)
    }

    private fun createNotificationChannel() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val channel = NotificationChannel(
                CHANNEL_ID,
                "Pomodoro Timer",
                NotificationManager.IMPORTANCE_LOW
            ).apply {
                description = "Shows the active pomodoro timer countdown"
                setShowBadge(false)
            }
            val manager = getSystemService(NotificationManager::class.java)
            manager.createNotificationChannel(channel)
        }
    }

    private fun buildNotification(): Notification {
        val mins = secondsLeft / 60
        val secs = secondsLeft % 60
        val timeText = String.format("%02d:%02d", mins, secs)

        val phaseLabel = when (phase) {
            "work" -> "Work"
            "shortBreak" -> "Short Break"
            "longBreak" -> "Long Break"
            else -> "Timer"
        }

        val contentTitle = if (taskName.isNotEmpty()) taskName else "Pomodoro Timer"
        val contentText = if (isPaused) "$phaseLabel — $timeText (Paused)" else "$phaseLabel — $timeText"

        // Launch app intent
        val launchIntent = packageManager.getLaunchIntentForPackage(packageName)
        val contentPendingIntent = PendingIntent.getActivity(
            this, 0, launchIntent,
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
        )

        val builder = NotificationCompat.Builder(this, CHANNEL_ID)
            .setSmallIcon(android.R.drawable.ic_media_play)
            .setContentTitle(contentTitle)
            .setContentText(contentText)
            .setContentIntent(contentPendingIntent)
            .setOngoing(true)
            .setOnlyAlertOnce(true)
            .setSilent(true)
            .setCategory(NotificationCompat.CATEGORY_PROGRESS)
            .setVisibility(NotificationCompat.VISIBILITY_PUBLIC)

        // Use Chronometer for live countdown
        if (!isPaused && secondsLeft > 0) {
            builder.setUsesChronometer(true)
            builder.setChronometerCountDown(true)
            builder.setWhen(System.currentTimeMillis() + secondsLeft * 1000L)
        }

        // Action buttons
        if (isPaused) {
            val resumeIntent = Intent(this, PomodoroForegroundService::class.java).apply {
                action = ACTION_RESUME
            }
            val resumePi = PendingIntent.getService(
                this, 1, resumeIntent,
                PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
            )
            builder.addAction(android.R.drawable.ic_media_play, "Resume", resumePi)
        } else {
            val pauseIntent = Intent(this, PomodoroForegroundService::class.java).apply {
                action = ACTION_PAUSE
            }
            val pausePi = PendingIntent.getService(
                this, 2, pauseIntent,
                PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
            )
            builder.addAction(android.R.drawable.ic_media_pause, "Pause", pausePi)
        }

        val stopIntent = Intent(this, PomodoroForegroundService::class.java).apply {
            action = ACTION_STOP
        }
        val stopPi = PendingIntent.getService(
            this, 3, stopIntent,
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
        )
        builder.addAction(android.R.drawable.ic_delete, "Stop", stopPi)

        return builder.build()
    }

    private fun updateNotification() {
        val manager = getSystemService(NotificationManager::class.java)
        manager.notify(NOTIFICATION_ID, buildNotification())
    }
}
