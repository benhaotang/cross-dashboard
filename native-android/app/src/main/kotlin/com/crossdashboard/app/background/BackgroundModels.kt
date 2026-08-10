package com.crossdashboard.app.background

import kotlinx.serialization.Serializable

@Serializable
enum class BackgroundSource { INBOX, VIEWS }

@Serializable
enum class WallpaperProfile { STANDARD, FOLD_COVER, FOLD_INNER }

@Serializable
enum class PreferredWallpaperOrientation { PORTRAIT, LANDSCAPE }

enum class WallpaperImageFit { SCALE, FILL, STRETCH }
enum class WallpaperImageSlot { BOTH, LIGHT, DARK }

data class WallpaperAppearance(
    val imagePath: String? = null,
    val lightImagePath: String? = null,
    val darkImagePath: String? = null,
    val glassOpacity: Float = 0.8f,
    val imageFit: WallpaperImageFit = WallpaperImageFit.FILL,
) {
    fun imagePath(dark: Boolean): String? = if (dark) darkImagePath ?: imagePath else lightImagePath ?: imagePath
    fun imagePath(slot: WallpaperImageSlot): String? = when (slot) {
        WallpaperImageSlot.BOTH -> imagePath
        WallpaperImageSlot.LIGHT -> lightImagePath
        WallpaperImageSlot.DARK -> darkImagePath
    }
}

@Serializable
data class BackgroundTemplate(
    val enabled: Boolean = true,
    val source: BackgroundSource,
    val inboxTypeFilter: String = "ALL",
    val inboxDateFilter: String = "ALL",
    val searchQuery: String? = null,
    val viewsTypeFilter: String = "ALL",
    val viewsDateFilter: String = "ALL",
    val viewsMode: String = "KANBAN",
    val capturedAt: Long = System.currentTimeMillis(),
)

data class BackgroundRow(
    val title: String,
    val subtitle: String,
    val kind: Kind,
    val group: String? = null,
    val overdue: Boolean = false,
) {
    enum class Kind { EVENT, TASK, ISSUE }
}

data class BackgroundContent(
    val title: String,
    val filterLabel: String,
    val mode: String? = null,
    val groups: List<String> = emptyList(),
    val rows: List<BackgroundRow>,
    val totalMinutes: Int = 0,
    val refreshedAt: Long = System.currentTimeMillis(),
)
