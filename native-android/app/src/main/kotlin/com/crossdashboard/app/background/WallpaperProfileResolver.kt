package com.crossdashboard.app.background

import android.content.Context
import android.view.Display
import com.crossdashboard.app.data.prefs.AppPreferences
import kotlinx.coroutines.flow.first

object WallpaperProfileResolver {
    suspend fun resolve(context: Context, prefs: AppPreferences): Pair<BackgroundTemplate?, PreferredWallpaperOrientation> {
        val standard = prefs.backgroundTemplateFlow(WallpaperProfile.STANDARD).first()
        val cover = prefs.backgroundTemplateFlow(WallpaperProfile.FOLD_COVER).first()
        val inner = prefs.backgroundTemplateFlow(WallpaperProfile.FOLD_INNER).first()
        val orientation = prefs.preferredWallpaperOrientationFlow.first()
        val isDefaultDisplay = context.display?.displayId == Display.DEFAULT_DISPLAY
        val selected = if (!isDefaultDisplay || (cover == null && inner == null)) standard else {
            if (context.resources.configuration.smallestScreenWidthDp < 600) cover ?: standard ?: inner
            else inner ?: standard ?: cover
        }
        return selected to orientation
    }
}
