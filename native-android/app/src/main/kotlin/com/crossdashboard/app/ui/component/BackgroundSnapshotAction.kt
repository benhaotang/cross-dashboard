package com.crossdashboard.app.ui.component

import android.app.WallpaperManager
import android.content.ComponentName
import android.content.Intent
import androidx.compose.foundation.layout.Row
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.outlined.PhotoCamera
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.platform.LocalContext
import com.crossdashboard.app.background.DashboardWallpaperService
import com.crossdashboard.app.background.WallpaperProfile
import com.crossdashboard.app.ui.adaptive.rememberFoldingFeature

@Composable
fun BackgroundSnapshotAction(onCapture: (List<WallpaperProfile>, () -> Unit) -> Unit) {
    val context = LocalContext.current
    val foldable = rememberFoldingFeature() != null ||
        context.packageManager.hasSystemFeature("android.hardware.sensor.hinge_angle")
    var chooseProfile by remember { mutableStateOf(false) }
    fun capture(profiles: List<WallpaperProfile>) = onCapture(profiles) {
        context.startActivity(Intent(WallpaperManager.ACTION_CHANGE_LIVE_WALLPAPER).apply {
            putExtra(WallpaperManager.EXTRA_LIVE_WALLPAPER_COMPONENT,
                ComponentName(context, DashboardWallpaperService::class.java))
        })
    }
    IconButton(onClick = { if (foldable) chooseProfile = true else capture(listOf(WallpaperProfile.STANDARD)) }) {
        Icon(Icons.Outlined.PhotoCamera, contentDescription = "Snapshot current view as background")
    }
    if (chooseProfile) AlertDialog(
        onDismissRequest = { chooseProfile = false },
        title = { Text("Background display") },
        text = { Text("Save this snapshot for the cover screen, opened screen, or both.") },
        confirmButton = { TextButton(onClick = { chooseProfile = false; capture(listOf(WallpaperProfile.FOLD_INNER)) }) { Text("Opened") } },
        dismissButton = {
            Row {
                TextButton(onClick = { chooseProfile = false; capture(listOf(WallpaperProfile.FOLD_COVER)) }) { Text("Cover") }
                TextButton(onClick = { chooseProfile = false; capture(listOf(WallpaperProfile.FOLD_COVER, WallpaperProfile.FOLD_INNER)) }) { Text("Both") }
            }
        },
    )
}
