package com.crossdashboard.app.ui.component

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.crossdashboard.app.domain.model.PomodoroPhase
import com.crossdashboard.app.ui.screen.tasks.PomodoroUiState
import com.crossdashboard.app.ui.screen.tasks.PomodoroViewModel

private val phaseBarColor: @Composable (PomodoroPhase) -> Color = { phase ->
    when (phase) {
        PomodoroPhase.WORK -> MaterialTheme.colorScheme.error
        PomodoroPhase.SHORT_BREAK -> MaterialTheme.colorScheme.tertiary
        PomodoroPhase.LONG_BREAK -> MaterialTheme.colorScheme.secondary
    }
}

/**
 * Compact floating mini-bar anchored at the bottom-end of the screen.
 * Visible when the Pomodoro timer is active but the full modal is hidden.
 *
 * Rendered at root-composable level (above AppNavigation) so it overlays all screens.
 */
@Composable
fun PomodoroBar(viewModel: PomodoroViewModel) {
    val state by viewModel.state.collectAsStateWithLifecycle()

    Box(
        modifier = Modifier
            .fillMaxSize()
            .systemBarsPadding()
            .padding(bottom = 80.dp, end = 16.dp),  // above nav bar
        contentAlignment = Alignment.BottomEnd,
    ) {
        PomodoroBarCard(
            state = state,
            onExpand = viewModel::showModal,
            onPauseResume = { if (state.running) viewModel.pause() else viewModel.resume() },
            onStop = viewModel::stop,
        )
    }
}

@Composable
private fun PomodoroBarCard(
    state: PomodoroUiState,
    onExpand: () -> Unit,
    onPauseResume: () -> Unit,
    onStop: () -> Unit,
) {
    val color = phaseBarColor(state.phase)
    val minutes = state.secondsLeft / 60
    val seconds = state.secondsLeft % 60
    val timeLabel = "%d:%02d".format(minutes, seconds)

    Surface(
        modifier = Modifier
            .width(220.dp)
            .clip(RoundedCornerShape(16.dp))
            .clickable { onExpand() },
        shape = RoundedCornerShape(16.dp),
        tonalElevation = 8.dp,
        shadowElevation = 6.dp,
    ) {
        Column(modifier = Modifier.fillMaxWidth()) {
            // Phase color bar
            Box(
                modifier = Modifier
                    .fillMaxWidth()
                    .height(4.dp)
                    .background(color)
            )

            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(horizontal = 12.dp, vertical = 8.dp),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Column(modifier = Modifier.weight(1f)) {
                    Text(
                        text = state.itemTitle,
                        style = MaterialTheme.typography.labelSmall,
                        maxLines = 1,
                        overflow = TextOverflow.Ellipsis,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                    Text(
                        text = timeLabel,
                        fontSize = 22.sp,
                        fontWeight = FontWeight.SemiBold,
                        color = color,
                    )
                    Text(
                        text = state.phase.label(),
                        style = MaterialTheme.typography.labelSmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                }

                // Play/pause
                TextButton(
                    onClick = onPauseResume,
                    modifier = Modifier.size(36.dp),
                    contentPadding = PaddingValues(0.dp),
                ) {
                    Text(
                        text = if (state.running) "⏸" else "▶",
                        fontSize = 16.sp,
                        color = color,
                    )
                }

                // Stop
                TextButton(
                    onClick = onStop,
                    modifier = Modifier.size(36.dp),
                    contentPadding = PaddingValues(0.dp),
                ) {
                    Text(
                        text = "⏹",
                        fontSize = 16.sp,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                }
            }
        }
    }
}
