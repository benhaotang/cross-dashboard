package com.crossdashboard.app.ui.component

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.layout.size
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import com.crossdashboard.app.data.prefs.CredentialKey
import com.crossdashboard.app.data.prefs.SecureStore
import com.crossdashboard.app.domain.model.CalDavCalendar
import kotlinx.serialization.json.Json
import javax.inject.Inject
import javax.inject.Singleton

/**
 * Resolves a calendarHref to its display color by reading the stored
 * CALDAV_SELECTED_CALENDARS JSON list from SecureStore.
 *
 * Injected as a singleton to avoid re-parsing the JSON on every composition.
 */
@Singleton
class CalendarColorResolver @Inject constructor(
    private val secureStore: SecureStore,
) {
    private var cachedCalendars: List<CalDavCalendar>? = null

    /** Returns the [Color] for a given calendar href, or null if not found. */
    fun resolve(calendarHref: String?): Color? {
        if (calendarHref == null) return null
        val hex = getCalendars().firstOrNull { it.href == calendarHref }?.color ?: return null
        return parseHexColor(hex)
    }

    /** Returns the display name for a given calendar href, or null if not found. */
    fun displayName(calendarHref: String?): String? {
        if (calendarHref == null) return null
        return getCalendars().firstOrNull { it.href == calendarHref }?.displayName
    }

    /** Invalidate the cache so the next call re-reads from SecureStore. */
    fun invalidate() {
        cachedCalendars = null
    }

    private fun getCalendars(): List<CalDavCalendar> {
        cachedCalendars?.let { return it }
        val json = secureStore.get(CredentialKey.CALDAV_SELECTED_CALENDARS) ?: return emptyList()
        return try {
            Json.decodeFromString<List<CalDavCalendar>>(json).also { cachedCalendars = it }
        } catch (_: Exception) {
            emptyList()
        }
    }
}

/** Parse a #RRGGBB hex string into a Compose [Color], returning null on error. */
fun parseHexColor(hex: String): Color? = try {
    val clean = hex.removePrefix("#")
    val rgb = clean.toLong(16)
    Color(
        red = ((rgb shr 16) and 0xFF).toInt() / 255f,
        green = ((rgb shr 8) and 0xFF).toInt() / 255f,
        blue = (rgb and 0xFF).toInt() / 255f,
    )
} catch (_: Exception) {
    null
}

/** Small colored circle to indicate which calendar an item belongs to. */
@Composable
fun CalendarColorDot(
    calendarHref: String?,
    resolver: CalendarColorResolver,
    dotSize: Dp = 8.dp,
    modifier: Modifier = Modifier,
) {
    val color = remember(calendarHref) { resolver.resolve(calendarHref) } ?: return
    val calendarName = remember(calendarHref) { resolver.displayName(calendarHref) }
    Canvas(
        modifier = modifier
            .size(dotSize)
            .semantics { contentDescription = calendarName?.let { "Calendar: $it" } ?: "Calendar color" },
    ) {
        drawCircle(color = color)
    }
}
