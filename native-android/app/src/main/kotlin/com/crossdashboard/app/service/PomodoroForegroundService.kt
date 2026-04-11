package com.crossdashboard.app.service

import android.app.*
import android.content.Intent
import android.net.Uri
import android.os.IBinder
import androidx.core.app.NotificationCompat
import com.crossdashboard.app.CrossDashboardApp.Companion.CHANNEL_POMODORO
import com.crossdashboard.app.MainActivity
import com.crossdashboard.app.R
import com.crossdashboard.app.domain.model.PomodoroPhase
import dagger.hilt.android.AndroidEntryPoint
import javax.inject.Inject

/**
 * Foreground service for the Pomodoro timer.
 *
 * Displays an Android 16 Live Update (promoted ongoing) notification using
 * Notification.ProgressStyle with a countdown chip in the status bar.
 *
 * The service receives commands via startService(Intent) with an ACTION_* extra,
 * rather than binding, to keep the API simple. PomodoroViewModel drives all
 * timer logic; this service only owns the notification lifecycle.
 *
 * Notification action buttons (Pause/Resume, Stop) post commands through
 * [PomodoroCommandBus] so PomodoroViewModel can update its state without
 * being in the foreground.
 */
@AndroidEntryPoint
class PomodoroForegroundService : Service() {

    @Inject lateinit var commandBus: PomodoroCommandBus

    // Mirror of the last known display state so pause/resume can rebuild
    // the notification without re-receiving all extras.
    private var currentTitle: String = "Pomodoro"
    private var currentPhase: PomodoroPhase = PomodoroPhase.WORK
    private var currentSecondsLeft: Int = 25 * 60
    private var currentlyRunning: Boolean = true

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        when (intent?.action) {
            ACTION_START -> {
                updateCurrentState(intent)
                startForeground(NOTIFICATION_ID, buildNotification())
            }
            ACTION_UPDATE -> {
                updateCurrentState(intent)
                notify(buildNotification())
            }
            ACTION_PAUSE -> {
                currentlyRunning = false
                notify(buildNotification())
                // Notify ViewModel after the notification is updated so the UI
                // and notification state stay in sync.
                commandBus.send(ACTION_PAUSE)
            }
            ACTION_RESUME -> {
                currentlyRunning = true
                notify(buildNotification())
                commandBus.send(ACTION_RESUME)
            }
            ACTION_STOP -> {
                // Tell the ViewModel to reset its state BEFORE the service dies so
                // the in-app bar/modal is removed and the CountDownTimer is cancelled.
                commandBus.send(ACTION_STOP)
                stopForeground(STOP_FOREGROUND_REMOVE)
                stopSelf()
            }
        }
        return START_NOT_STICKY
    }

    private fun updateCurrentState(intent: Intent) {
        currentTitle = intent.getStringExtra(EXTRA_TITLE) ?: currentTitle
        val phaseName = intent.getStringExtra(EXTRA_PHASE) ?: currentPhase.name
        currentPhase = runCatching { PomodoroPhase.valueOf(phaseName) }.getOrDefault(currentPhase)
        currentSecondsLeft = intent.getIntExtra(EXTRA_SECONDS_LEFT, currentSecondsLeft)
        currentlyRunning = intent.getBooleanExtra(EXTRA_RUNNING, currentlyRunning)
    }

    private fun notify(notification: Notification) {
        val nm = getSystemService(NOTIFICATION_SERVICE) as NotificationManager
        nm.notify(NOTIFICATION_ID, notification)
    }

    private fun buildNotification(): Notification {
        val phaseLabel = currentPhase.label()
        val phaseTotal = when (currentPhase) {
            PomodoroPhase.WORK -> 25 * 60
            PomodoroPhase.SHORT_BREAK -> 5 * 60
            PomodoroPhase.LONG_BREAK -> 15 * 60
        }
        val progress = ((phaseTotal - currentSecondsLeft).toFloat() / phaseTotal * 100)
            .toInt().coerceIn(0, 100)

        // Chronometer counts down to this epoch — only meaningful when running.
        val endTimeMs = System.currentTimeMillis() + currentSecondsLeft * 1000L

        // When paused, include the frozen remaining time in the content text so
        // the user can still see how much time is left without a live countdown.
        val timeLabel = "%d:%02d".format(currentSecondsLeft / 60, currentSecondsLeft % 60)
        val contentText = if (currentlyRunning) phaseLabel
                          else "$phaseLabel · $timeLabel ${getString(R.string.pomodoro_paused_suffix)}"

        // Tap → re-open app and surface the Pomodoro bar/modal
        val contentIntent = PendingIntent.getActivity(
            this, REQUEST_CONTENT,
            Intent(this, MainActivity::class.java).apply {
                action = Intent.ACTION_VIEW
                data = Uri.parse(DEEP_LINK_POMODORO)
                flags = Intent.FLAG_ACTIVITY_SINGLE_TOP
            },
            PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT,
        )

        // Pause / Resume action — toggles based on running state
        val pauseResumeAction = if (currentlyRunning) {
            val pi = PendingIntent.getService(
                this, REQUEST_PAUSE,
                Intent(this, PomodoroForegroundService::class.java).apply { action = ACTION_PAUSE },
                PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT,
            )
            NotificationCompat.Action(R.drawable.ic_pause, getString(R.string.pomodoro_pause), pi)
        } else {
            val pi = PendingIntent.getService(
                this, REQUEST_RESUME,
                Intent(this, PomodoroForegroundService::class.java).apply { action = ACTION_RESUME },
                PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT,
            )
            NotificationCompat.Action(R.drawable.ic_play, getString(R.string.pomodoro_resume), pi)
        }

        val stopPendingIntent = PendingIntent.getService(
            this, REQUEST_STOP,
            Intent(this, PomodoroForegroundService::class.java).apply { action = ACTION_STOP },
            PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT,
        )

        return NotificationCompat.Builder(this, CHANNEL_POMODORO)
            .setSmallIcon(R.drawable.ic_timer)
            .setContentTitle(currentTitle)
            .setContentText(contentText)
            .setOngoing(true)
            .setOnlyAlertOnce(true)
            // Chronometer and its countdown chip are only active while the timer is running.
            // Setting setUsesChronometer(false) when paused removes the live chip from the
            // status bar so it no longer ticks while the session is paused.
            .setShowWhen(currentlyRunning)
            .setWhen(if (currentlyRunning) endTimeMs else 0L)
            .setUsesChronometer(currentlyRunning)
            .setChronometerCountDown(currentlyRunning)
            // Live Update / promoted ongoing (Android 16 API 36)
            .setRequestPromotedOngoing(true)
            .setProgress(100, progress, false)
            .setContentIntent(contentIntent)
            .addAction(pauseResumeAction)
            .addAction(R.drawable.ic_stop, getString(R.string.pomodoro_stop), stopPendingIntent)
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
        const val EXTRA_RUNNING = "running"

        const val DEEP_LINK_POMODORO = "crossdashboard://pomodoro"

        private const val REQUEST_CONTENT = 0
        private const val REQUEST_PAUSE = 1
        private const val REQUEST_RESUME = 2
        private const val REQUEST_STOP = 3
    }
}
