package expo.modules.widget

import android.app.AlarmManager
import android.appwidget.AppWidgetManager
import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.net.Uri
import android.os.Build
import android.provider.Settings
import androidx.work.Constraints
import androidx.work.ExistingPeriodicWorkPolicy
import androidx.work.NetworkType
import androidx.work.PeriodicWorkRequestBuilder
import androidx.work.WorkManager
import expo.modules.kotlin.modules.Module
import expo.modules.kotlin.modules.ModuleDefinition
import java.util.concurrent.TimeUnit

class DashboardWidgetModule : Module() {
    override fun definition() = ModuleDefinition {
        Name("DashboardWidget")

        /**
         * Store pre-formatted widget rows (from fresh JS-side sync data).
         * eventRows / taskRows: pipe-separated strings of up to 3 entries each.
         */
        Function("updateWidgetData") { eventRowsStr: String, taskRowsStr: String, issuesCount: Int, lastSync: String ->
            val context = appContext.reactContext ?: return@Function false

            val eventParts = eventRowsStr.split("|").map { it.trim() }
            val taskParts = taskRowsStr.split("|").map { it.trim() }

            val prefs = context.getSharedPreferences("cross_dashboard_widget", Context.MODE_PRIVATE)
            prefs.edit()
                .putString("event_row_0", eventParts.getOrNull(0) ?: "")
                .putString("event_row_1", eventParts.getOrNull(1) ?: "")
                .putString("event_row_2", eventParts.getOrNull(2) ?: "")
                .putInt("events_count", eventParts.count { it.isNotEmpty() })
                .putString("task_row_0", taskParts.getOrNull(0) ?: "")
                .putString("task_row_1", taskParts.getOrNull(1) ?: "")
                .putString("task_row_2", taskParts.getOrNull(2) ?: "")
                .putInt("tasks_count", taskParts.count { it.isNotEmpty() })
                .putInt("issues_count", issuesCount)
                .putString("last_sync", lastSync)
                .apply()

            triggerRefresh(context)
            true
        }

        /**
         * Save credentials for the background WorkManager sync worker.
         * calendarHrefs and giteaRepos are pipe-separated lists.
         */
        Function("saveWorkerCredentials") { caldavServer: String, caldavUser: String, caldavPass: String,
                                            calendarHrefs: String, giteaUrl: String, giteaToken: String,
                                            giteaRepos: String ->
            val context = appContext.reactContext ?: return@Function false
            val prefs = context.getSharedPreferences("cross_dashboard_widget", Context.MODE_PRIVATE)
            prefs.edit()
                .putString("worker_caldav_server", caldavServer)
                .putString("worker_caldav_user", caldavUser)
                .putString("worker_caldav_pass", caldavPass)
                .putString("worker_calendar_hrefs", calendarHrefs)
                .putString("worker_gitea_url", giteaUrl)
                .putString("worker_gitea_token", giteaToken)
                .putString("worker_gitea_repos", giteaRepos)
                .apply()
            true
        }

        /**
         * Schedule (or reschedule) the periodic WorkManager sync.
         * intervalMinutes: minimum 15, recommended 60.
         */
        Function("scheduleSync") { intervalMinutes: Int ->
            val context = appContext.reactContext ?: return@Function false
            val clamped = intervalMinutes.coerceAtLeast(15)
            val prefs = context.getSharedPreferences("cross_dashboard_widget", Context.MODE_PRIVATE)
            prefs.edit().putInt("worker_sync_interval", clamped).apply()
            val constraints = Constraints.Builder()
                .setRequiredNetworkType(NetworkType.CONNECTED)
                .build()
            val request = PeriodicWorkRequestBuilder<WidgetSyncWorker>(
                clamped.toLong(), TimeUnit.MINUTES
            )
                .setConstraints(constraints)
                .build()
            WorkManager.getInstance(context).enqueueUniquePeriodicWork(
                WidgetSyncWorker.WORK_NAME,
                ExistingPeriodicWorkPolicy.UPDATE,
                request
            )
            true
        }

        /**
         * Cancel the periodic background sync.
         */
        Function("cancelSync") {
            val context = appContext.reactContext ?: return@Function false
            WorkManager.getInstance(context).cancelUniqueWork(WidgetSyncWorker.WORK_NAME)
            true
        }

        Function("forceRefresh") {
            val context = appContext.reactContext ?: return@Function false
            val manager = AppWidgetManager.getInstance(context)
            val ids = manager.getAppWidgetIds(
                ComponentName(context, DashboardWidgetProvider::class.java)
            )
            for (id in ids) {
                val options = manager.getAppWidgetOptions(id)
                DashboardWidgetProvider.updateWidget(context, manager, id, options)
            }
            true
        }

        /**
         * Save notification settings for the background worker.
         * If enabled, ensures the periodic worker is running.
         */
        Function("saveWorkerNotificationSettings") { enabled: Boolean, minutesBefore: Int ->
            val context = appContext.reactContext ?: return@Function false
            val prefs = context.getSharedPreferences("cross_dashboard_widget", Context.MODE_PRIVATE)
            prefs.edit()
                .putString("notif_enabled", if (enabled) "true" else "false")
                .putString("notif_minutes", minutesBefore.toString())
                .apply()

            if (enabled) {
                val intervalMinutes = prefs.getInt("worker_sync_interval", 60)
                val constraints = Constraints.Builder()
                    .setRequiredNetworkType(NetworkType.CONNECTED)
                    .build()
                val request = PeriodicWorkRequestBuilder<WidgetSyncWorker>(
                    intervalMinutes.toLong(), TimeUnit.MINUTES
                )
                    .setConstraints(constraints)
                    .build()
                WorkManager.getInstance(context).enqueueUniquePeriodicWork(
                    WidgetSyncWorker.WORK_NAME,
                    ExistingPeriodicWorkPolicy.KEEP,
                    request
                )
            }
            true
        }

        /**
         * Returns whether the app can schedule exact alarms (API 31+).
         */
        Function("canScheduleExactAlarms") {
            val context = appContext.reactContext ?: return@Function true
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                val alarmManager = context.getSystemService(Context.ALARM_SERVICE) as AlarmManager
                alarmManager.canScheduleExactAlarms()
            } else {
                true
            }
        }

        /**
         * Opens the system "Alarms & Reminders" settings page for this app.
         */
        Function("requestExactAlarmPermission") {
            val context = appContext.reactContext ?: return@Function false
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                val intent = Intent(Settings.ACTION_REQUEST_SCHEDULE_EXACT_ALARM).apply {
                    data = Uri.parse("package:${context.packageName}")
                    addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
                }
                context.startActivity(intent)
                true
            } else {
                true
            }
        }
    }

    private fun triggerRefresh(context: Context) {
        val intent = Intent(context, DashboardWidgetProvider::class.java)
        intent.action = DashboardWidgetProvider.ACTION_UPDATE_WIDGET
        context.sendBroadcast(intent)
    }
}
