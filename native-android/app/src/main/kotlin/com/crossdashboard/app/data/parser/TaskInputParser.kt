package com.crossdashboard.app.data.parser

import com.crossdashboard.app.domain.model.*
import java.time.*
import java.time.temporal.TemporalAdjusters
import java.util.Locale

/**
 * Intelligent single-line task input parser (Todoist-style).
 *
 * Syntax:
 *   !! meet friends #social tonight    → priority=medium, tag=social, due=today 21:00
 *   !!! deploy hotfix tomorrow morning → priority=high, due=tomorrow 08:00
 *   buy milk #errands                  → no priority, tag=errands, no due
 *   call mom monday                    → due=next Monday 10:00
 *
 * Priority markers:
 *   !   → low (priority=9)
 *   !!  → medium (priority=5)
 *   !!! → high (priority=1)
 *
 * Time keywords (case-insensitive):
 *   today, tonight, tomorrow, tomorrow morning/afternoon/night,
 *   monday-sunday (always future), next week
 */
object TaskInputParser {

    fun parse(
        input: String,
        defaults: TaskDefaults = TaskDefaults(),
        now: LocalDateTime = LocalDateTime.now(),
    ): ParsedTask {
        var text = input.trim()

        // ─── Priority ────────────────────────────────────────────────────────
        val priority = when {
            text.startsWith("!!!") -> { text = text.removePrefix("!!!").trim(); 1 }
            text.startsWith("!!") -> { text = text.removePrefix("!!").trim(); 5 }
            text.startsWith("!") -> { text = text.removePrefix("!").trim(); 9 }
            else -> 0
        }

        // ─── Tags ────────────────────────────────────────────────────────────
        val tagRegex = Regex("""#(\w+)""")
        val tags = tagRegex.findAll(text).map { it.groupValues[1].lowercase() }.toList()
        text = tagRegex.replace(text, "").trim()
        // Collapse multiple spaces
        text = text.replace(Regex("""\s{2,}"""), " ").trim()

        // ─── Due date ─────────────────────────────────────────────────────────
        val due = extractDue(text, defaults, now)
        // Remove the time keyword from summary
        if (due != null) {
            text = stripTimeKeywords(text).trim()
        }

        return ParsedTask(
            summary = text,
            priority = priority,
            categories = tags,
            due = due?.atZone(ZoneId.systemDefault())?.toInstant(),
        )
    }

    private fun extractDue(
        text: String,
        d: TaskDefaults,
        now: LocalDateTime,
    ): LocalDateTime? {
        val lower = text.lowercase(Locale.getDefault())
        val today = now.toLocalDate()

        // tomorrow morning/afternoon/night
        if (lower.contains("tomorrow morning"))
            return LocalDateTime.of(today.plusDays(1), LocalTime.of(d.morningHour, 0))
        if (lower.contains("tomorrow afternoon"))
            return LocalDateTime.of(today.plusDays(1), LocalTime.of(d.afternoonHour, 0))
        if (lower.contains("tomorrow night"))
            return LocalDateTime.of(today.plusDays(1), LocalTime.of(d.nightHour, 0))
        if (lower.contains("tomorrow"))
            return LocalDateTime.of(today.plusDays(1), LocalTime.of(d.defaultHour, 0))

        // tonight
        if (lower.contains("tonight"))
            return LocalDateTime.of(today, LocalTime.of(d.nightHour, 0))

        // today
        if (lower.contains("today"))
            return LocalDateTime.of(today, LocalTime.of(d.defaultHour, 0))

        // next week
        if (lower.contains("next week"))
            return LocalDateTime.of(today.plusWeeks(1), LocalTime.of(d.defaultHour, 0))

        // weekday names — always next occurrence (never same day or past)
        val weekdays = mapOf(
            "monday" to DayOfWeek.MONDAY,
            "tuesday" to DayOfWeek.TUESDAY,
            "wednesday" to DayOfWeek.WEDNESDAY,
            "thursday" to DayOfWeek.THURSDAY,
            "friday" to DayOfWeek.FRIDAY,
            "saturday" to DayOfWeek.SATURDAY,
            "sunday" to DayOfWeek.SUNDAY,
        )
        for ((keyword, dow) in weekdays) {
            if (lower.contains(keyword)) {
                var target = today.with(TemporalAdjusters.nextOrSame(dow))
                if (!target.isAfter(today)) target = target.plusWeeks(1)
                return LocalDateTime.of(target, LocalTime.of(d.defaultHour, 0))
            }
        }

        return null
    }

    private fun stripTimeKeywords(text: String): String {
        val keywords = listOf(
            "tomorrow morning", "tomorrow afternoon", "tomorrow night", "tomorrow",
            "tonight", "today", "next week",
            "monday", "tuesday", "wednesday", "thursday", "friday", "saturday", "sunday",
        )
        var result = text
        for (kw in keywords) {
            result = result.replace(Regex("""\b${Regex.escape(kw)}\b""", RegexOption.IGNORE_CASE), "")
        }
        return result.replace(Regex("""\s{2,}"""), " ").trim()
    }
}
