package com.crossdashboard.app.ui

import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.hilt.navigation.compose.hiltViewModel
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.crossdashboard.app.domain.model.ThemePreference
import com.crossdashboard.app.SharePayload
import com.crossdashboard.app.ui.adaptive.FoldAwareContent
import com.crossdashboard.app.ui.adaptive.hingeAwarePadding
import com.crossdashboard.app.ui.component.BiometricLockScreen
import com.crossdashboard.app.ui.component.PomodoroBar
import com.crossdashboard.app.ui.component.PomodoroModal
import com.crossdashboard.app.ui.navigation.AppNavigation
import com.crossdashboard.app.ui.screen.memos.MemosViewModel
import com.crossdashboard.app.ui.screen.memos.ShareCaptureSheet
import com.crossdashboard.app.ui.screen.tasks.PomodoroViewModel
import com.crossdashboard.app.ui.theme.CrossDashboardTheme
import com.crossdashboard.app.ui.viewmodel.AppViewModel

@Composable
fun CrossDashboardRoot(
    pendingAction: String?,
    onActionConsumed: () -> Unit,
    pendingSharePayload: SharePayload? = null,
    onShareConsumed: () -> Unit = {},
) {
    val appVm: AppViewModel = hiltViewModel()
    val pomodoroVm: PomodoroViewModel = hiltViewModel()
    val memosVm: MemosViewModel = hiltViewModel()

    val theme by appVm.theme.collectAsStateWithLifecycle()
    val visibleScreens by appVm.visibleScreens.collectAsStateWithLifecycle()
    val biometricEnabled by appVm.biometricLock.collectAsStateWithLifecycle()
    val systemCredentialEnabled by appVm.systemCredential.collectAsStateWithLifecycle()
    val pomodoroState by pomodoroVm.state.collectAsStateWithLifecycle()

    // Handle notification tap: crossdashboard://pomodoro → show the running timer
    LaunchedEffect(pendingAction) {
        if (pendingAction == "pomodoro") {
            pomodoroVm.showModal()
            onActionConsumed()
        }
    }

    val isDark = when (theme) {
        ThemePreference.DARK -> true
        ThemePreference.LIGHT -> false
        ThemePreference.SYSTEM -> isSystemInDarkTheme()
    }

    CrossDashboardTheme(darkTheme = isDark) {
        // PIN lock gate — shown on cold launch when the user has enabled the app PIN lock
        if (biometricEnabled && !appVm.isUnlocked) {
            BiometricLockScreen(
                onUnlocked = { appVm.unlock() },
                systemCredentialEnabled = systemCredentialEnabled,
            )
            return@CrossDashboardTheme
        }

        // Foldable posture awareness — content is inset to avoid the hinge crease.
        // On non-foldable devices this is a no-op.
        FoldAwareContent { _ ->
            AppNavigation(
                modifier = Modifier
                    .fillMaxSize()
                    .hingeAwarePadding(),
                visibleScreens = visibleScreens,
                pendingAction = pendingAction,
                onActionConsumed = onActionConsumed,
            )
        }

        // Pomodoro overlays — always on top of the nav scaffold
        if (pomodoroState.active && pomodoroState.modalVisible) {
            PomodoroModal(viewModel = pomodoroVm)
        } else if (pomodoroState.active) {
            PomodoroBar(viewModel = pomodoroVm)
        }

        // Share capture overlay — shown when app is opened via system share sheet
        if (pendingSharePayload != null) {
            ShareCaptureSheet(
                sharedText = pendingSharePayload.text,
                sharedAttachments = pendingSharePayload.attachments,
                onDismiss = onShareConsumed,
                onCapture = { content, visibility, attachments ->
                    memosVm.createMemo(content, visibility, attachments)
                    onShareConsumed()
                },
            )
        }
    }
}
