package com.crossdashboard.app

import android.app.Application
import android.app.NotificationChannel
import android.app.NotificationManager
import androidx.core.content.getSystemService
import androidx.hilt.work.HiltWorkerFactory
import androidx.work.Configuration
import dagger.hilt.android.HiltAndroidApp
import javax.inject.Inject

@HiltAndroidApp
class CrossDashboardApp : Application(), Configuration.Provider {

    @Inject lateinit var workerFactory: HiltWorkerFactory

    override val workManagerConfiguration: Configuration
        get() = Configuration.Builder()
            .setWorkerFactory(workerFactory)
            .build()

    override fun onCreate() {
        super.onCreate()
        createNotificationChannels()
    }

    private fun createNotificationChannels() {
        val nm = getSystemService<NotificationManager>() ?: return

        nm.createNotificationChannels(
            listOf(
                NotificationChannel(
                    CHANNEL_POMODORO,
                    getString(R.string.channel_pomodoro_name),
                    NotificationManager.IMPORTANCE_HIGH,
                ).apply {
                    description = getString(R.string.channel_pomodoro_desc)
                    setShowBadge(false)
                },
                NotificationChannel(
                    CHANNEL_EVENTS,
                    getString(R.string.channel_events_name),
                    NotificationManager.IMPORTANCE_HIGH,
                ).apply {
                    description = getString(R.string.channel_events_desc)
                },
                NotificationChannel(
                    CHANNEL_SYNC,
                    getString(R.string.channel_sync_name),
                    NotificationManager.IMPORTANCE_LOW,
                ).apply {
                    description = getString(R.string.channel_sync_desc)
                    setShowBadge(false)
                },
            )
        )
    }

    companion object {
        const val CHANNEL_POMODORO = "pomodoro"
        const val CHANNEL_EVENTS = "event_reminders"
        const val CHANNEL_SYNC = "sync_status"
    }
}
