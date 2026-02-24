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

class DashboardWidgetProvider : AppWidgetProvider() {

    override fun onUpdate(context: Context, appWidgetManager: AppWidgetManager, appWidgetIds: IntArray) {
        for (widgetId in appWidgetIds) {
            val options = appWidgetManager.getAppWidgetOptions(widgetId)
            updateWidget(context, appWidgetManager, widgetId, options)
        }
    }

    override fun onAppWidgetOptionsChanged(
        context: Context,
        appWidgetManager: AppWidgetManager,
        appWidgetId: Int,
        newOptions: Bundle
    ) {
        updateWidget(context, appWidgetManager, appWidgetId, newOptions)
    }

    override fun onReceive(context: Context, intent: Intent) {
        super.onReceive(context, intent)
        if (intent.action == ACTION_UPDATE_WIDGET) {
            val manager = AppWidgetManager.getInstance(context)
            val ids = manager.getAppWidgetIds(
                ComponentName(context, DashboardWidgetProvider::class.java)
            )
            for (id in ids) {
                val options = manager.getAppWidgetOptions(id)
                updateWidget(context, manager, id, options)
            }
        }
    }

    companion object {
        const val ACTION_UPDATE_WIDGET = "expo.modules.widget.UPDATE_WIDGET"
        private const val PREFS_NAME = "cross_dashboard_widget"

        /**
         * Estimate how many content rows fit per section given widget height in dp.
         * Each row is ~14dp + section header ~16dp + footer ~14dp + padding ~20dp.
         */
        private fun rowsPerSection(heightDp: Int): Int {
            // Total overhead: events header(16) + tasks header(22) + footer(16) + padding(20) = 74dp
            val available = heightDp - 74
            // Each event row ≈ 15dp, tasks header adds another 6dp gap
            return when {
                available >= 90 -> 3
                available >= 60 -> 2
                available >= 30 -> 1
                else -> 1
            }
        }

        fun updateWidget(context: Context, manager: AppWidgetManager, widgetId: Int, options: Bundle?) {
            val prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)

            // Determine available height
            val minHeight = options?.getInt(AppWidgetManager.OPTION_APPWIDGET_MIN_HEIGHT, 110) ?: 110
            val maxRows = rowsPerSection(minHeight)

            val eventRows = listOf(
                prefs.getString("event_row_0", "") ?: "",
                prefs.getString("event_row_1", "") ?: "",
                prefs.getString("event_row_2", "") ?: ""
            )
            val taskRows = listOf(
                prefs.getString("task_row_0", "") ?: "",
                prefs.getString("task_row_1", "") ?: "",
                prefs.getString("task_row_2", "") ?: ""
            )
            val issuesCount = prefs.getInt("issues_count", 0)
            val lastSync = prefs.getString("last_sync", "Not synced") ?: "Not synced"

            val views = RemoteViews(context.packageName, R.layout.widget_dashboard)

            // ── Event rows ────────────────────────────────────────────────────
            val eventIds = listOf(R.id.widget_event_1, R.id.widget_event_2, R.id.widget_event_3)
            var shownEvents = 0
            for (i in 0 until 3) {
                val text = eventRows[i]
                val visible = i < maxRows && text.isNotEmpty()
                views.setViewVisibility(eventIds[i], if (visible) View.VISIBLE else View.GONE)
                if (visible) {
                    views.setTextViewText(eventIds[i], text)
                    shownEvents++
                }
            }
            views.setViewVisibility(
                R.id.widget_events_empty,
                if (shownEvents == 0) View.VISIBLE else View.GONE
            )

            // ── Task rows ─────────────────────────────────────────────────────
            val taskIds = listOf(R.id.widget_task_1, R.id.widget_task_2, R.id.widget_task_3)
            var shownTasks = 0
            for (i in 0 until 3) {
                val text = taskRows[i]
                val visible = i < maxRows && text.isNotEmpty()
                views.setViewVisibility(taskIds[i], if (visible) View.VISIBLE else View.GONE)
                if (visible) {
                    views.setTextViewText(taskIds[i], text)
                    shownTasks++
                }
            }
            views.setViewVisibility(
                R.id.widget_tasks_empty,
                if (shownTasks == 0) View.VISIBLE else View.GONE
            )

            // ── Footer ────────────────────────────────────────────────────────
            val issuesLabel = if (issuesCount > 0) "● $issuesCount open issues" else ""
            views.setTextViewText(R.id.widget_issues_label, issuesLabel)
            views.setTextViewText(R.id.widget_last_sync, lastSync)

            // ── FAB: open app to Tasks screen with add action ──────────────────
            val fabIntent = Intent(Intent.ACTION_VIEW, Uri.parse("crossdashboard://tasks?action=add"))
            fabIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TOP)
            val fabPending = PendingIntent.getActivity(
                context, 1, fabIntent,
                PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
            )
            views.setOnClickPendingIntent(R.id.widget_fab, fabPending)

            // ── Tap widget body: open app ──────────────────────────────────────
            val openIntent = context.packageManager.getLaunchIntentForPackage(context.packageName)
            if (openIntent != null) {
                openIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
                val openPending = PendingIntent.getActivity(
                    context, 0, openIntent,
                    PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
                )
                views.setOnClickPendingIntent(R.id.widget_content, openPending)
            }

            manager.updateAppWidget(widgetId, views)
        }
    }
}
