package com.crossdashboard.app.service

import android.app.*
import android.content.Intent
import android.os.IBinder
import androidx.core.app.NotificationCompat
import com.crossdashboard.app.CrossDashboardApp.Companion.CHANNEL_POMODORO
import com.crossdashboard.app.MainActivity
import com.crossdashboard.app.R
import com.crossdashboard.app.domain.model.PomodoroPhase
import dagger.hilt.android.AndroidEntryPoint

/**
 * Foreground service for the Pomodoro timer.
 *
 * Displays an Android 16 Live Update (promoted ongoing) notification using
 * Notification.ProgressStyle with a countdown chip in the status bar.
 *
 * The service receives commands via startService(Intent) with an ACTION_* extra,
 * rather than binding, to keep the API simple. PomodoroViewModel drives all
 * timer logic; this service only owns the notification lifecycle.
 */
@AndroidEntryPoint
class PomodoroForegroundService : Service() {

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        when (intent?.action) {
            ACTION_START -> {
                val taskTitle = intent.getStringExtra(EXTRA_TITLE) ?: "Pomodoro"
                val phaseName = intent.getStringExtra(EXTRA_PHASE) ?: PomodoroPhase.WORK.name
                val secondsLeft = intent.getIntExtra(EXTRA_SECONDS_LEFT, 25 * 60)
                val phase = runCatching { PomodoroPhase.valueOf(phaseName) }.getOrDefault(PomodoroPhase.WORK)
                val notification = buildNotification(taskTitle, phase, secondsLeft)
                startForeground(NOTIFICATION_ID, notification)
            }
            ACTION_UPDATE -> {
                val taskTitle = intent.getStringExtra(EXTRA_TITLE) ?: "Pomodoro"
                val phaseName = intent.getStringExtra(EXTRA_PHASE) ?: PomodoroPhase.WORK.name
                val secondsLeft = intent.getIntExtra(EXTRA_SECONDS_LEFT, 0)
                val phase = runCatching { PomodoroPhase.valueOf(phaseName) }.getOrDefault(PomodoroPhase.WORK)
                val nm = getSystemService(NOTIFICATION_SERVICE) as NotificationManager
                nm.notify(NOTIFICATION_ID, buildNotification(taskTitle, phase, secondsLeft))
            }
            ACTION_STOP -> {
                stopForeground(STOP_FOREGROUND_REMOVE)
                stopSelf()
            }
        }
        return START_STICKY
    }

    private fun buildNotification(
        taskTitle: String,
        phase: PomodoroPhase,
        secondsLeft: Int,
    ): Notification {
        val phaseLabel = phase.label()
        val phaseTotal = when (phase) {
            PomodoroPhase.WORK -> 25 * 60
            PomodoroPhase.SHORT_BREAK -> 5 * 60
            PomodoroPhase.LONG_BREAK -> 15 * 60
        }
        val progress = ((phaseTotal - secondsLeft).toFloat() / phaseTotal * 100).toInt()
        val endTimeMs = System.currentTimeMillis() + secondsLeft * 1000L

        // Content intent — tap to open app
        val contentIntent = PendingIntent.getActivity(
            this, 0,
            Intent(this, MainActivity::class.java),
            PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT,
        )

        // Action: Pause
        val pauseIntent = PendingIntent.getService(
            this, 1,
            Intent(this, PomodoroForegroundService::class.java).apply { action = ACTION_PAUSE },
            PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT,
        )

        // Action: Stop
        val stopIntent = PendingIntent.getService(
            this, 2,
            Intent(this, PomodoroForegroundService::class.java).apply { action = ACTION_STOP },
            PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT,
        )

        return NotificationCompat.Builder(this, CHANNEL_POMODORO)
            .setSmallIcon(R.drawable.ic_timer)
            .setContentTitle(taskTitle)
            .setContentText(phaseLabel)
            .setOngoing(true)
            .setOnlyAlertOnce(true)
            .setShowWhen(true)
            .setWhen(endTimeMs)
            .setUsesChronometer(true)
            .setChronometerCountDown(true)
            // Live Update / promoted ongoing (Android 16 API 36)
            .setRequestPromotedOngoing(true)
            // Progress bar — also shows in the notification body
            .setProgress(100, progress, false)
            .setContentIntent(contentIntent)
            .addAction(R.drawable.ic_pause, getString(R.string.pomodoro_pause), pauseIntent)
            .addAction(R.drawable.ic_stop, getString(R.string.pomodoro_stop), stopIntent)
            .build()
    }

    companion object {
        const val NOTIFICATION_ID = 1001

        const val ACTION_START = "com.crossdashboard.app.POMODORO_START"
        const val ACTION_UPDATE = "com.crossdashboard.app.POMODORO_UPDATE"
        const val ACTION_STOP = "com.crossdashboard.app.POMODORO_STOP"
        const val ACTION_PAUSE = "com.crossdashboard.app.POMODORO_PAUSE"
        const val ACTION_RESUME = "com.crossdashboard.app.POMODORO_RESUME"

        const val EXTRA_TITLE = "title"
        const val EXTRA_PHASE = "phase"
        const val EXTRA_SECONDS_LEFT = "seconds_left"
    }
}
