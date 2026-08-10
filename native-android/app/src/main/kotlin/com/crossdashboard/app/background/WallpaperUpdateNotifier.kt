package com.crossdashboard.app.background

import android.content.Context
import android.content.Intent

object WallpaperUpdateNotifier {
    const val ACTION = "com.crossdashboard.app.action.WALLPAPER_DATA_CHANGED"
    fun notify(context: Context) {
        context.sendBroadcast(Intent(ACTION).setPackage(context.packageName))
    }
}
