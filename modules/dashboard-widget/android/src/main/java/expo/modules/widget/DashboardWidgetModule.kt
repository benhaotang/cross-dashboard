package expo.modules.widget

import android.appwidget.AppWidgetManager
import android.content.ComponentName
import android.content.Context
import android.content.Intent
import expo.modules.kotlin.modules.Module
import expo.modules.kotlin.modules.ModuleDefinition

class DashboardWidgetModule : Module() {
    override fun definition() = ModuleDefinition {
        Name("DashboardWidget")

        Function("updateWidgetData") { eventsCount: Int, issuesCount: Int, nextEvent: String, lastSync: String ->
            val context = appContext.reactContext ?: return@Function false

            val prefs = context.getSharedPreferences("cross_dashboard_widget", Context.MODE_PRIVATE)
            prefs.edit()
                .putInt("events_count", eventsCount)
                .putInt("issues_count", issuesCount)
                .putString("next_event", nextEvent)
                .putString("last_sync", lastSync)
                .apply()

            // Trigger widget refresh
            val intent = Intent(context, DashboardWidgetProvider::class.java)
            intent.action = DashboardWidgetProvider.ACTION_UPDATE_WIDGET
            context.sendBroadcast(intent)

            true
        }

        Function("forceRefresh") {
            val context = appContext.reactContext ?: return@Function false
            val manager = AppWidgetManager.getInstance(context)
            val ids = manager.getAppWidgetIds(
                ComponentName(context, DashboardWidgetProvider::class.java)
            )
            for (id in ids) {
                DashboardWidgetProvider.updateWidget(context, manager, id)
            }
            true
        }
    }
}
