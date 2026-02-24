package expo.modules.widget

import android.appwidget.AppWidgetManager
import android.appwidget.AppWidgetProvider
import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.widget.RemoteViews

class DashboardWidgetProvider : AppWidgetProvider() {

    override fun onUpdate(context: Context, appWidgetManager: AppWidgetManager, appWidgetIds: IntArray) {
        for (widgetId in appWidgetIds) {
            updateWidget(context, appWidgetManager, widgetId)
        }
    }

    override fun onReceive(context: Context, intent: Intent) {
        super.onReceive(context, intent)
        if (intent.action == ACTION_UPDATE_WIDGET) {
            val manager = AppWidgetManager.getInstance(context)
            val ids = manager.getAppWidgetIds(
                ComponentName(context, DashboardWidgetProvider::class.java)
            )
            onUpdate(context, manager, ids)
        }
    }

    companion object {
        const val ACTION_UPDATE_WIDGET = "expo.modules.widget.UPDATE_WIDGET"
        private const val PREFS_NAME = "cross_dashboard_widget"

        fun updateWidget(context: Context, manager: AppWidgetManager, widgetId: Int) {
            val prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
            val eventsCount = prefs.getInt("events_count", 0)
            val issuesCount = prefs.getInt("issues_count", 0)
            val nextEvent = prefs.getString("next_event", "No upcoming events") ?: "No upcoming events"
            val lastSync = prefs.getString("last_sync", "Not synced") ?: "Not synced"

            val views = RemoteViews(context.packageName, R.layout.widget_dashboard)
            views.setTextViewText(R.id.widget_events_count, eventsCount.toString())
            views.setTextViewText(R.id.widget_issues_count, issuesCount.toString())
            views.setTextViewText(R.id.widget_next_event, nextEvent)
            views.setTextViewText(R.id.widget_last_sync, lastSync)

            manager.updateAppWidget(widgetId, views)
        }
    }
}
