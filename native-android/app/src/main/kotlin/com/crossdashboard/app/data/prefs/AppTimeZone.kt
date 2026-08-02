package com.crossdashboard.app.data.prefs

import java.time.ZoneId
import java.util.TimeZone

/** Applies the optional app-wide timezone override while retaining the OS timezone as the default. */
object AppTimeZone {
    val systemZoneId: ZoneId = ZoneId.systemDefault()

    fun applyOverride(zoneId: String?) {
        val zone = zoneId
            ?.trim()
            ?.takeIf { it.isNotEmpty() }
            ?.let { runCatching { ZoneId.of(it) }.getOrNull() }

        // A null default restores Android's live OS timezone behavior.
        TimeZone.setDefault(zone?.let { TimeZone.getTimeZone(it) })
    }
}
