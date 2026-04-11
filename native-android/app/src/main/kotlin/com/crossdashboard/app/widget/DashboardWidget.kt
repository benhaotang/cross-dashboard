package com.crossdashboard.app.widget

import android.content.Context
import android.content.Intent
import android.net.Uri
import androidx.compose.runtime.Composable
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
 * Uses [SizeMode.Exact] so [LocalSize.current] always reflects the actual allocated widget
 * dimensions. Row counts for events and tasks are computed dynamically from the available
 * height, filling all free space rather than capping at a fixed tier.
 *
 * Fixed overhead (dp): outer padding 16 + events header 14 + tasks header 14 + spacer 6 +
 * footer 36 = 86dp. Each content row is ~14dp (12sp text + 2dp vertical padding).
 * Total per-section rows = floor((height − 86) / 14 / 2), clamped to [1, data.size].
 *
 * Tapping anywhere on the widget body opens [MainActivity]. The FAB deep-links to the
 * Tasks quick-add screen instead.
 */
class DashboardWidget : GlanceAppWidget() {

    override val stateDefinition = DashboardWidgetStateDefinition

    // Exact mode gives the real allocated dp size at every resize step.
    override val sizeMode: SizeMode = SizeMode.Exact

    override suspend fun provideGlance(context: Context, id: GlanceId) {
        provideContent {
            val state = currentState<DashboardWidgetState>()
            val size = LocalSize.current

            // Compute how many rows we can fit, split evenly between the two sections.
            val fixedOverheadDp = 86f   // padding + 2 headers + spacer + footer
            val rowHeightDp = 14f
            val totalRows = ((size.height.value - fixedOverheadDp) / rowHeightDp)
                .toInt()
                .coerceAtLeast(2)
            // Give each section its fair share; if odd, events gets the extra row.
            val eventRows = (totalRows + 1) / 2
            val taskRows = totalRows / 2

            DashboardWidgetContent(
                state = state,
                maxEventRows = eventRows,
                maxTaskRows = taskRows,
            )
        }
    }
}

@Composable
private fun DashboardWidgetContent(
    state: DashboardWidgetState,
    maxEventRows: Int,
    maxTaskRows: Int,
) {
    GlanceTheme {
        // Outer box: tapping anywhere opens MainActivity.
        Box(
            modifier = GlanceModifier
                .fillMaxSize()
                .background(GlanceTheme.colors.surface)
                .padding(8.dp)
                .clickable(
                    actionStartActivity(
                        Intent(Intent.ACTION_MAIN).apply {
                            addCategory(Intent.CATEGORY_LAUNCHER)
                            setClassName(
                                "com.crossdashboard.app",
                                "com.crossdashboard.app.MainActivity",
                            )
                            flags = Intent.FLAG_ACTIVITY_NEW_TASK or
                                    Intent.FLAG_ACTIVITY_RESET_TASK_IF_NEEDED
                        }
                    )
                ),
        ) {
            Column(modifier = GlanceModifier.fillMaxSize()) {
                // ── Events section ───────────────────────────────────────────
                SectionHeader(label = "EVENTS")
                if (state.eventRows.isEmpty()) {
                    RowText(text = "No upcoming events", muted = true)
                } else {
                    state.eventRows.take(maxEventRows).forEach { row ->
                        RowText(text = row)
                    }
                }

                Spacer(modifier = GlanceModifier.height(6.dp))

                // ── Tasks section ────────────────────────────────────────────
                SectionHeader(label = "TASKS")
                if (state.taskRows.isEmpty()) {
                    RowText(text = "No tasks due", muted = true)
                } else {
                    state.taskRows.take(maxTaskRows).forEach { row ->
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

                    // FAB: deep-link to Tasks screen with quick-add.
                    // Its own clickable overrides the outer body tap for this area.
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
