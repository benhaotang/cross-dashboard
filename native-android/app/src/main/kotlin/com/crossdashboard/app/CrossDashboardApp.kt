package com.crossdashboard.app

import android.app.Application
import android.app.NotificationChannel
import android.app.NotificationManager
import androidx.core.content.getSystemService
import androidx.hilt.work.HiltWorkerFactory
import androidx.work.Configuration
import androidx.work.ExistingPeriodicWorkPolicy
import androidx.work.WorkManager
import com.crossdashboard.app.worker.SyncWorker
import com.crossdashboard.app.data.prefs.AppPreferences
import com.crossdashboard.app.data.prefs.AppTimeZone
import dagger.hilt.android.HiltAndroidApp
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.runBlocking
import javax.inject.Inject

@HiltAndroidApp
class CrossDashboardApp : Application(), Configuration.Provider {

    @Inject lateinit var workerFactory: HiltWorkerFactory
    @Inject lateinit var appPreferences: AppPreferences

    override val workManagerConfiguration: Configuration
        get() = Configuration.Builder()
            .setWorkerFactory(workerFactory)
            .build()

    override fun onCreate() {
        super.onCreate()
        // Apply before repositories, workers, and UI parse or format cached timestamps.
        runBlocking { AppTimeZone.applyOverride(appPreferences.timeZoneOverrideFlow.first()) }
        createNotificationChannels()
        ensurePeriodicSyncScheduled()
    }

    /**
     * Enqueues the periodic sync worker on every app start with [ExistingPeriodicWorkPolicy.KEEP]
     * so it is registered on first launch (or after a data clear) without overriding an interval
     * the user has already configured via Settings (which uses [ExistingPeriodicWorkPolicy.UPDATE]).
     */
    private fun ensurePeriodicSyncScheduled() {
        WorkManager.getInstance(this).enqueueUniquePeriodicWork(
            SyncWorker.WORK_NAME_PERIODIC,
            ExistingPeriodicWorkPolicy.KEEP,
            SyncWorker.periodicRequest(60),
        )
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
                    CHANNEL_TASKS,
                    getString(R.string.channel_tasks_name),
                    NotificationManager.IMPORTANCE_HIGH,
                ).apply {
                    description = getString(R.string.channel_tasks_desc)
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
        const val CHANNEL_TASKS = "task_reminders"
        const val CHANNEL_SYNC = "sync_status"
    }
}
