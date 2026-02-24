package expo.modules.widget

import android.app.PendingIntent
import android.appwidget.AppWidgetManager
import android.appwidget.AppWidgetProvider
import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.view.View
import android.widget.RemoteViews
import androidx.work.Constraints
import androidx.work.NetworkType
import androidx.work.OneTimeWorkRequestBuilder
import androidx.work.WorkManager

class MainDashboardWidgetProvider : AppWidgetProvider() {

    override fun onUpdate(context: Context, appWidgetManager: AppWidgetManager, appWidgetIds: IntArray) {
        // Trigger a one-shot sync so data is fresh when displayed
        enqueueSync(context)
        for (widgetId in appWidgetIds) {
            updateWidget(context, appWidgetManager, widgetId)
        }
    }

    override fun onAppWidgetOptionsChanged(
        context: Context,
        appWidgetManager: AppWidgetManager,
        appWidgetId: Int,
        newOptions: Bundle
    ) {
        updateWidget(context, appWidgetManager, appWidgetId)
    }

    override fun onReceive(context: Context, intent: Intent) {
        super.onReceive(context, intent)
        if (intent.action == DashboardWidgetProvider.ACTION_UPDATE_WIDGET) {
            val manager = AppWidgetManager.getInstance(context)
            val ids = manager.getAppWidgetIds(
                ComponentName(context, MainDashboardWidgetProvider::class.java)
            )
            for (id in ids) {
                updateWidget(context, manager, id)
            }
        }
    }

    companion object {
        private const val PREFS_NAME = "cross_dashboard_widget"

        private fun enqueueSync(context: Context) {
            val constraints = Constraints.Builder()
                .setRequiredNetworkType(NetworkType.CONNECTED)
                .build()
            val request = OneTimeWorkRequestBuilder<WidgetSyncWorker>()
                .setConstraints(constraints)
                .build()
            WorkManager.getInstance(context).enqueue(request)
        }

        fun updateWidget(context: Context, manager: AppWidgetManager, widgetId: Int) {
            val prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
            val views = RemoteViews(context.packageName, R.layout.widget_main)

            // ── Upcoming events ──────────────────────────────────────────────
            val eventIds = listOf(R.id.main_event_1, R.id.main_event_2, R.id.main_event_3)
            var shownEvents = 0
            for (i in 0..2) {
                val text = prefs.getString("event_row_$i", "") ?: ""
                val visible = text.isNotEmpty()
                views.setViewVisibility(eventIds[i], if (visible) View.VISIBLE else View.GONE)
                if (visible) {
                    views.setTextViewText(eventIds[i], text)
                    shownEvents++
                }
            }
            views.setViewVisibility(
                R.id.main_events_empty,
                if (shownEvents == 0) View.VISIBLE else View.GONE
            )

            // ── Overdue tasks ─────────────────────────────────────────────────
            val overdueIds = listOf(R.id.main_overdue_1, R.id.main_overdue_2, R.id.main_overdue_3)
            var shownOverdue = 0
            for (i in 0..2) {
                val text = prefs.getString("overdue_task_row_$i", "") ?: ""
                val visible = text.isNotEmpty()
                views.setViewVisibility(overdueIds[i], if (visible) View.VISIBLE else View.GONE)
                if (visible) {
                    views.setTextViewText(overdueIds[i], text)
                    shownOverdue++
                }
            }
            views.setViewVisibility(
                R.id.main_overdue_empty,
                if (shownOverdue == 0) View.VISIBLE else View.GONE
            )

            // ── Stats ──────────────────────────────────────────────────────────
            val eventsToday = prefs.getInt("events_remaining_today", 0)
            val pomodoroToday = prefs.getInt("pomodoro_sessions_today", 0)
            val issuesCount = prefs.getInt("issues_count", 0)
            views.setTextViewText(R.id.stat_events_count, eventsToday.toString())
            views.setTextViewText(R.id.stat_pomodoro_count, pomodoroToday.toString())
            views.setTextViewText(R.id.stat_issues_count, issuesCount.toString())

            // ── Pen FAB: open app to Tasks quick-add ───────────────────────────
            val penIntent = Intent(Intent.ACTION_VIEW, Uri.parse("crossdashboard://tasks?action=add"))
            penIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TOP)
            val penPending = PendingIntent.getActivity(
                context, 10, penIntent,
                PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
            )
            views.setOnClickPendingIntent(R.id.main_pen_fab, penPending)

            // ── Body tap: open app ─────────────────────────────────────────────
            val openIntent = context.packageManager.getLaunchIntentForPackage(context.packageName)
            if (openIntent != null) {
                openIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
                val openPending = PendingIntent.getActivity(
                    context, 11, openIntent,
                    PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
                )
                views.setOnClickPendingIntent(R.id.widget_main_content, openPending)
                views.setOnClickPendingIntent(R.id.stat_events_btn, openPending)
                views.setOnClickPendingIntent(R.id.stat_pomodoro_btn, openPending)
                views.setOnClickPendingIntent(R.id.stat_issues_btn, openPending)
            }

            manager.updateAppWidget(widgetId, views)
        }
    }
}
