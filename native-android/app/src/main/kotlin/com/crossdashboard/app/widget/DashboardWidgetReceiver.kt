package com.crossdashboard.app.widget

import androidx.glance.appwidget.GlanceAppWidgetReceiver

/**
 * AppWidget broadcast receiver for [DashboardWidget].
 * Registered in AndroidManifest with APPWIDGET_UPDATE intent-filter
 * and `@xml/widget_info` metadata.
 *
 * WorkManager-driven updates — [updatePeriodMillis] is set to 0 in widget_info.xml.
 */
class DashboardWidgetReceiver : GlanceAppWidgetReceiver() {
    override val glanceAppWidget = DashboardWidget()
}
