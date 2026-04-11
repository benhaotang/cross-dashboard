package com.crossdashboard.app.widget

import android.content.Context
import android.content.Intent
import android.net.Uri
import androidx.compose.runtime.Composable
import androidx.compose.ui.unit.DpSize
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.glance.*
import androidx.glance.GlanceTheme
import androidx.glance.action.clickable
import androidx.glance.appwidget.GlanceAppWidget
import androidx.glance.appwidget.SizeMode
import androidx.glance.appwidget.action.actionStartActivity
import androidx.glance.appwidget.cornerRadius
import androidx.glance.appwidget.provideContent
import androidx.glance.layout.*
import androidx.glance.text.FontWeight
import androidx.glance.text.Text
import androidx.glance.text.TextStyle

/**
 * Glance home screen widget for Cross-Dashboard.
 *
 * Content is driven by [DashboardWidgetState] stored in [DashboardWidgetStateDefinition].
 * The state is updated by [SyncWorker] after each background sync.
 *
 * Uses [SizeMode.Responsive] with three explicit size tiers so Android calls
 * [provideContent] with the exact allocated size — correct at every resize step
 * without needing to infer row counts from a single height value.
 *
 * Tiers:
 *   - SMALL  (≥250×110dp): 1 event row + 1 task row + footer
 *   - MEDIUM (≥250×180dp): 2 event rows + 2 task rows + footer
 *   - LARGE  (≥250×250dp): 3 event rows + 3 task rows + footer
 */
class DashboardWidget : GlanceAppWidget() {

    override val stateDefinition = DashboardWidgetStateDefinition

    override val sizeMode: SizeMode = SizeMode.Responsive(
        setOf(SMALL, MEDIUM, LARGE)
    )

    override suspend fun provideGlance(context: Context, id: GlanceId) {
        provideContent {
            val state = currentState<DashboardWidgetState>()
            val size = LocalSize.current
            val visibleRows = when {
                size.height >= LARGE.height -> 3
                size.height >= MEDIUM.height -> 2
                else -> 1
            }
            DashboardWidgetContent(state = state, visibleRows = visibleRows)
        }
    }

    companion object {
        val SMALL = DpSize(250.dp, 110.dp)
        val MEDIUM = DpSize(250.dp, 180.dp)
        val LARGE = DpSize(250.dp, 250.dp)
    }
}

@Composable
private fun DashboardWidgetContent(state: DashboardWidgetState, visibleRows: Int) {
    GlanceTheme {
        Box(
            modifier = GlanceModifier
                .fillMaxSize()
                .background(GlanceTheme.colors.surface)
                .padding(8.dp),
        ) {
            Column(modifier = GlanceModifier.fillMaxSize()) {
                // ── Events section ───────────────────────────────────────────
                SectionHeader(label = "EVENTS")
                if (state.eventRows.isEmpty()) {
                    RowText(text = "No upcoming events", muted = true)
                } else {
                    state.eventRows.take(visibleRows).forEach { row ->
                        RowText(text = row)
                    }
                }

                Spacer(modifier = GlanceModifier.height(6.dp))

                // ── Tasks section ────────────────────────────────────────────
                SectionHeader(label = "TASKS")
                if (state.taskRows.isEmpty()) {
                    RowText(text = "No tasks due", muted = true)
                } else {
                    state.taskRows.take(visibleRows).forEach { row ->
                        RowText(text = row)
                    }
                }

                Spacer(modifier = GlanceModifier.defaultWeight())

                // ── Footer: issues count + last sync + FAB ───────────────────
                Row(
                    modifier = GlanceModifier.fillMaxWidth(),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Text(
                        text = buildString {
                            if (state.issuesCount > 0) append("⬤ ${state.issuesCount} issues  ·  ")
                            append(state.lastSync)
                        },
                        style = TextStyle(
                            color = GlanceTheme.colors.onSurfaceVariant,
                            fontSize = 10.sp,
                        ),
                        modifier = GlanceModifier.defaultWeight(),
                        maxLines = 1,
                    )

                    // FAB: deep-link to Tasks screen with quick-add
                    Box(
                        modifier = GlanceModifier
                            .size(32.dp)
                            .cornerRadius(16.dp)
                            .background(GlanceTheme.colors.primary)
                            .clickable(
                                actionStartActivity(
                                    Intent(Intent.ACTION_VIEW, Uri.parse("crossdashboard://tasks?action=add_task"))
                                )
                            ),
                        contentAlignment = Alignment.Center,
                    ) {
                        Text(
                            text = "+",
                            style = TextStyle(
                                color = GlanceTheme.colors.onPrimary,
                                fontSize = 18.sp,
                                fontWeight = FontWeight.Bold,
                            ),
                        )
                    }
                }
            }
        }
    }
}

@Composable
private fun SectionHeader(label: String) {
    Text(
        text = label,
        style = TextStyle(
            color = GlanceTheme.colors.primary,
            fontSize = 10.sp,
            fontWeight = FontWeight.Bold,
        ),
        modifier = GlanceModifier.padding(bottom = 2.dp),
    )
}

@Composable
private fun RowText(text: String, muted: Boolean = false) {
    Text(
        text = text,
        style = TextStyle(
            color = if (muted) GlanceTheme.colors.onSurfaceVariant else GlanceTheme.colors.onSurface,
            fontSize = 12.sp,
        ),
        maxLines = 1,
        modifier = GlanceModifier.padding(vertical = 1.dp),
    )
}
