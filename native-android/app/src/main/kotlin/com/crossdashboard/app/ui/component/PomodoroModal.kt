package com.crossdashboard.app.ui.component

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Close
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.window.Dialog
import androidx.compose.ui.window.DialogProperties
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.crossdashboard.app.domain.model.PomodoroPhase
import com.crossdashboard.app.ui.screen.tasks.PomodoroUiState
import com.crossdashboard.app.ui.screen.tasks.PomodoroViewModel

/**
 * Full-screen Pomodoro modal.
 * Shows a large countdown, phase indicator, session progress dots,
 * and play/pause, skip, and stop controls.
 */
@Composable
fun PomodoroModal(viewModel: PomodoroViewModel) {
    val state by viewModel.state.collectAsStateWithLifecycle()

    Dialog(
        onDismissRequest = viewModel::hideModal,
        properties = DialogProperties(usePlatformDefaultWidth = false),
    ) {
        Surface(
            modifier = Modifier.fillMaxSize(),
            color = MaterialTheme.colorScheme.background,
        ) {
            PomodoroModalContent(
                state = state,
                onClose = viewModel::hideModal,
                onPauseResume = { if (state.running) viewModel.pause() else viewModel.resume() },
                onSkip = viewModel::skipPhase,
                onStop = viewModel::stop,
            )
        }
    }
}

@Composable
private fun PomodoroModalContent(
    state: PomodoroUiState,
    onClose: () -> Unit,
    onPauseResume: () -> Unit,
    onSkip: () -> Unit,
    onStop: () -> Unit,
) {
    val phaseColor = when (state.phase) {
        PomodoroPhase.WORK -> MaterialTheme.colorScheme.error
        PomodoroPhase.SHORT_BREAK -> MaterialTheme.colorScheme.tertiary
        PomodoroPhase.LONG_BREAK -> MaterialTheme.colorScheme.secondary
    }

    val minutes = state.secondsLeft / 60
    val seconds = state.secondsLeft % 60
    val timeLabel = "%d:%02d".format(minutes, seconds)

    Column(
        modifier = Modifier
            .fillMaxSize()
            .padding(24.dp)
            .systemBarsPadding(),
        horizontalAlignment = Alignment.CenterHorizontally,
    ) {
        // Close / collapse
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.End,
        ) {
            IconButton(onClick = onClose) {
                Icon(Icons.Filled.Close, contentDescription = "Collapse")
            }
        }

        Spacer(Modifier.weight(0.5f))

        // Phase label chip
        Surface(
            shape = MaterialTheme.shapes.large,
            color = phaseColor.copy(alpha = 0.12f),
        ) {
            Text(
                text = state.phase.label(),
                modifier = Modifier.padding(horizontal = 16.dp, vertical = 6.dp),
                style = MaterialTheme.typography.labelLarge,
                color = phaseColor,
            )
        }

        Spacer(Modifier.height(32.dp))

        // Task title
        if (state.itemTitle.isNotBlank()) {
            Text(
                text = state.itemTitle,
                style = MaterialTheme.typography.titleMedium,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
                textAlign = TextAlign.Center,
                maxLines = 2,
            )
            Spacer(Modifier.height(16.dp))
        }

        // Large countdown
        Text(
            text = timeLabel,
            fontSize = 80.sp,
            fontWeight = FontWeight.Light,
            color = phaseColor,
            textAlign = TextAlign.Center,
        )

        Spacer(Modifier.height(32.dp))

        // Session dots
        SessionDots(
            completedSessions = state.completedSessions,
            sessionsUntilLong = state.settings.sessionsUntilLongBreak,
            currentPhase = state.phase,
            dotColor = phaseColor,
        )

        Spacer(Modifier.weight(1f))

        // Controls
        Row(
            horizontalArrangement = Arrangement.spacedBy(24.dp),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            // Stop
            FilledTonalIconButton(
                onClick = onStop,
                modifier = Modifier.size(56.dp),
            ) {
                Icon(Icons.Filled.Close, contentDescription = "Stop", modifier = Modifier.size(28.dp))
            }

            // Play / Pause — primary action
            FloatingActionButton(
                onClick = onPauseResume,
                modifier = Modifier.size(72.dp),
                containerColor = phaseColor,
                contentColor = MaterialTheme.colorScheme.onError,
            ) {
                Text(
                    text = if (state.running) "⏸" else "▶",
                    fontSize = 28.sp,
                )
            }

            // Skip to next phase
            FilledTonalIconButton(
                onClick = onSkip,
                modifier = Modifier.size(56.dp),
            ) {
                Text("⏭", fontSize = 22.sp)
            }
        }

        Spacer(Modifier.height(32.dp))
    }
}

@Composable
private fun SessionDots(
    completedSessions: Int,
    sessionsUntilLong: Int,
    currentPhase: PomodoroPhase,
    dotColor: Color,
) {
    Row(
        horizontalArrangement = Arrangement.spacedBy(8.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        val cycleDone = completedSessions % sessionsUntilLong
        repeat(sessionsUntilLong) { index ->
            val filled = index < cycleDone || (index == cycleDone && currentPhase != PomodoroPhase.WORK)
            Box(
                modifier = Modifier
                    .size(10.dp)
                    .clip(CircleShape)
                    .background(if (filled) dotColor else dotColor.copy(alpha = 0.25f))
            )
        }
    }
}
