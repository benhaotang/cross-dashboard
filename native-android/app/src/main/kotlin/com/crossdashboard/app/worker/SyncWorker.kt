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

    override suspend fun doWork(): Result {
        val calendarHrefs = selectedCalendarHrefs()
        val giteaRepos = giteaRepositories()

        // Each source is isolated: one failure must not prevent the others from running
        // or block the widget/alarm update at the end.
        coroutineScope {
            val events = async {
                runCatching { eventRepository.sync(calendarHrefs) }
                    .onFailure { e -> Log.e(TAG, "Event sync failed", e) }
            }
            val tasks = async {
                runCatching { taskRepository.sync(calendarHrefs) }
                    .onFailure { e -> Log.e(TAG, "Task sync failed", e) }
            }
            val notes = async {
                runCatching { noteRepository.sync(calendarHrefs) }
                    .onFailure { e -> Log.e(TAG, "Note sync failed", e) }
            }
            val issues = async {
                if (giteaRepos.isNotEmpty()) {
                    runCatching { issueRepository.sync(giteaRepos) }
                        .onFailure { e -> Log.e(TAG, "Issue sync failed", e) }
                }
            }
            events.await()
            tasks.await()
            notes.await()
            issues.await()
        }

        // Always reschedule alarms regardless of whether syncs succeeded
        runCatching {
            val upcoming = eventRepository.getUpcoming(limit = 50)
            alarmScheduler.rescheduleAll(upcoming)
        }.onFailure { e -> Log.e(TAG, "Alarm reschedule failed", e) }

        // Always update widget and persist timestamp — this is the only way the user
        // sees that a sync cycle ran, even if all network calls returned errors.
        runCatching {
            updateWidgetState()
            prefs.setLastSync(System.currentTimeMillis())
        }.onFailure { e -> Log.e(TAG, "Widget/lastSync update failed", e) }

        return Result.success()
    }

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

        // No network constraint: an explicit "Sync Now" tap should run immediately.
        // Each repository sync handles its own errors gracefully.
        fun oneTimeRequest(): OneTimeWorkRequest =
            OneTimeWorkRequestBuilder<SyncWorker>()
                .build()
    }
}
