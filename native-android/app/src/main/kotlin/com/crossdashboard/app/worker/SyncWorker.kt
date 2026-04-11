package com.crossdashboard.app.worker

import android.content.Context
import androidx.glance.appwidget.GlanceAppWidgetManager
import androidx.hilt.work.HiltWorker
import androidx.work.*
import com.crossdashboard.app.alarm.EventAlarmScheduler
import com.crossdashboard.app.data.prefs.AppPreferences
import com.crossdashboard.app.data.prefs.CredentialKey
import com.crossdashboard.app.data.prefs.SecureStore
import com.crossdashboard.app.data.repository.*
import com.crossdashboard.app.widget.DashboardWidget
import com.crossdashboard.app.widget.DashboardWidgetState
import com.crossdashboard.app.widget.WidgetStateStore
import dagger.assisted.Assisted
import dagger.assisted.AssistedInject
import kotlinx.coroutines.async
import kotlinx.coroutines.coroutineScope
import kotlinx.coroutines.flow.first
import kotlinx.serialization.json.Json
import android.util.Log
import java.time.Instant
import java.time.ZoneId
import java.time.format.DateTimeFormatter
import com.crossdashboard.app.domain.model.CalDavCalendar
import com.crossdashboard.app.domain.model.TaskStatus

/**
 * WorkManager [CoroutineWorker] that syncs all repositories in parallel,
 * updates event alarms, and refreshes the Glance widget state.
 *
 * Scheduled by:
 * - [BootReceiver] on boot / package replaced (one-shot)
 * - [SettingsScreen] via WorkManager.enqueueUniquePeriodicWork (periodic)
 * - "Sync Now" button in Settings (one-shot)
 */
@HiltWorker
class SyncWorker @AssistedInject constructor(
    @Assisted private val context: Context,
    @Assisted workerParams: WorkerParameters,
    private val eventRepository: EventRepository,
    private val taskRepository: TaskRepository,
    private val noteRepository: NoteRepository,
    private val issueRepository: IssueRepository,
    private val secureStore: SecureStore,
    private val prefs: AppPreferences,
    private val alarmScheduler: EventAlarmScheduler,
) : CoroutineWorker(context, workerParams) {

    override suspend fun doWork(): Result = runCatching {
        val calendarHrefs = selectedCalendarHrefs()
        val giteaRepos = giteaRepositories()

        // Sync all sources in parallel
        coroutineScope {
            val events = async { eventRepository.sync(calendarHrefs) }
            val tasks = async { taskRepository.sync(calendarHrefs) }
            val notes = async { noteRepository.sync(calendarHrefs) }
            val issues = async {
                if (giteaRepos.isNotEmpty()) issueRepository.sync(giteaRepos)
            }
            events.await()
            tasks.await()
            notes.await()
            issues.await()
        }

        // Reschedule event alarms
        val upcomingEvents = eventRepository.getUpcoming(limit = 50)
        alarmScheduler.rescheduleAll(upcomingEvents)

        // Update Glance widget state
        updateWidgetState()

        // Persist last sync timestamp
        prefs.setLastSync(System.currentTimeMillis())
    }.fold(
        onSuccess = { Result.success() },
        onFailure = { e ->
            Log.e(TAG, "Sync failed (attempt ${runAttemptCount + 1})", e)
            if (runAttemptCount < 2) Result.retry() else Result.failure()
        }
    )

    private fun selectedCalendarHrefs(): List<String> {
        val raw = secureStore.get(CredentialKey.CALDAV_SELECTED_CALENDARS) ?: return emptyList()
        return runCatching {
            Json.decodeFromString<List<CalDavCalendar>>(raw).map { it.href }
        }.getOrDefault(emptyList())
    }

    private fun giteaRepositories(): List<String> {
        val raw = secureStore.get(CredentialKey.GITEA_REPOS) ?: return emptyList()
        return raw.split(",").map { it.trim() }.filter { it.isNotBlank() }
    }

    private suspend fun updateWidgetState() {
        val now = Instant.now()
        val formatter = DateTimeFormatter.ofPattern("MM/dd HH:mm").withZone(ZoneId.systemDefault())
        val dateFormatter = DateTimeFormatter.ofPattern("MM/dd").withZone(ZoneId.systemDefault())

        val events = eventRepository.getUpcoming(limit = 3)
        val eventRows = events.map { event ->
            val timeStr = formatter.format(event.start)
            "$timeStr ${event.summary}"
        }

        val tasks = taskRepository.getDueSoon(
            deadlineEpoch = now.plusSeconds(7 * 24 * 3600).toEpochMilli(),
            limit = 3,
        ).filter { it.status != TaskStatus.COMPLETED }

        val taskRows = tasks.map { task ->
            val overdue = task.due != null && task.due.isBefore(now)
            val prefix = if (overdue) "⚠ " else ""
            val dueStr = task.due?.let { dateFormatter.format(it) + " " } ?: ""
            "$prefix$dueStr${task.summary}"
        }

        val openIssuesCount = issueRepository.openIssues.first().size
        val lastSyncFormatted = DateTimeFormatter
            .ofPattern("HH:mm")
            .withZone(ZoneId.systemDefault())
            .format(now)

        val state = DashboardWidgetState(
            eventRows = eventRows,
            taskRows = taskRows,
            issuesCount = openIssuesCount,
            lastSync = lastSyncFormatted,
        )

        WidgetStateStore.update(context, state)
        GlanceAppWidgetManager(context).getGlanceIds(DashboardWidget::class.java).forEach { id ->
            DashboardWidget().update(context, id)
        }
    }

    companion object {
        private const val TAG = "SyncWorker"
        const val WORK_NAME_PERIODIC = "sync_periodic"
        const val WORK_NAME_ONCE = "sync_once"

        fun periodicRequest(intervalMinutes: Int): PeriodicWorkRequest =
            PeriodicWorkRequestBuilder<SyncWorker>(
                intervalMinutes.coerceAtLeast(15).toLong(),
                java.util.concurrent.TimeUnit.MINUTES,
            )
                .setConstraints(Constraints(requiredNetworkType = NetworkType.CONNECTED))
                .build()

        fun oneTimeRequest(): OneTimeWorkRequest =
            OneTimeWorkRequestBuilder<SyncWorker>()
                .setConstraints(Constraints(requiredNetworkType = NetworkType.CONNECTED))
                .build()
    }
}
