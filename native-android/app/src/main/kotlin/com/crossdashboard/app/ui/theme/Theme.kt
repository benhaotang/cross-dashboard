package com.crossdashboard.app.ui.theme

import android.app.Activity
import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.dynamicDarkColorScheme
import androidx.compose.material3.dynamicLightColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.SideEffect
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalView
import androidx.core.view.WindowCompat

/**
 * CrossDashboard Material3 theme.
 *
 * - Dynamic color (Material You) via [dynamicDarkColorScheme]/[dynamicLightColorScheme].
 * - Status bar and navigation bar colours are transparent — set by [enableEdgeToEdge] in
 *   [MainActivity.onCreate]. Only the appearance-light flags are toggled here so system
 *   icons render correctly in both light and dark modes.
 * - Dark mode is driven by the caller ([CrossDashboardRoot] reads the user's theme
 *   preference from DataStore).
 */
@Composable
fun CrossDashboardTheme(
    darkTheme: Boolean = isSystemInDarkTheme(),
    content: @Composable () -> Unit,
) {
    val context = LocalContext.current
    val colorScheme = if (darkTheme) {
        dynamicDarkColorScheme(context)
    } else {
        dynamicLightColorScheme(context)
    }

    // Toggle status-bar icon tint (light icons in dark mode, dark icons in light mode).
    // Navigation bar appearance is also handled here; bar colours remain transparent
    // from enableEdgeToEdge().
    val view = LocalView.current
    if (!view.isInEditMode) {
        SideEffect {
            val window = (view.context as Activity).window
            val insetsController = WindowCompat.getInsetsController(window, view)
            insetsController.isAppearanceLightStatusBars = !darkTheme
            insetsController.isAppearanceLightNavigationBars = !darkTheme
        }
    }

    MaterialTheme(
        colorScheme = colorScheme,
        typography = AppTypography,
        shapes = AppShapes,
        content = content,
    )
}
