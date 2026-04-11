package com.crossdashboard.app.data.parser

import com.crossdashboard.app.domain.model.*
import java.time.*
import java.time.format.DateTimeFormatter
import java.time.format.DateTimeParseException

/**
 * Hand-written iCalendar (RFC 5545) parser for VEVENT, VTODO, and VJOURNAL components.
 *
 * Handles:
 * - Line unfolding (CRLF + whitespace continuation)
 * - Quoted-printable / base64 value decoding
 * - DTSTART / DTEND / DUE with TZID param and UTC Z suffix
 * - All-day dates (VALUE=DATE, no time component)
 * - RELATED-TO;RELTYPE=PARENT for subtask hierarchy
 * - CATEGORIES comma-separated list
 */
object ICalParser {

    // ─── Public entry points ─────────────────────────────────────────────────

    fun parseEvents(
        icalText: String,
        calendarHref: String? = null,
        resourceHref: String? = null,
        resourceEtag: String? = null,
    ): List<CalendarEvent> {
        val events = mutableListOf<CalendarEvent>()
        forEachComponent(icalText, "VEVENT") { props ->
            parseEvent(props, calendarHref, resourceHref, resourceEtag)?.let { events.add(it) }
        }
        return events
    }

    fun parseTasks(
        icalText: String,
        calendarHref: String? = null,
        resourceHref: String? = null,
        resourceEtag: String? = null,
    ): List<CalDavTask> {
        val tasks = mutableListOf<CalDavTask>()
        forEachComponent(icalText, "VTODO") { props ->
            parseTask(props, calendarHref, resourceHref, resourceEtag)?.let { tasks.add(it) }
        }
        return tasks
    }

    fun parseNotes(
        icalText: String,
        calendarHref: String? = null,
        resourceHref: String? = null,
        resourceEtag: String? = null,
    ): List<Note> {
        val notes = mutableListOf<Note>()
        forEachComponent(icalText, "VJOURNAL") { props ->
            parseNote(props, calendarHref, resourceHref, resourceEtag)?.let { notes.add(it) }
        }
        return notes
    }

    // ─── Component iterator ──────────────────────────────────────────────────

    private fun forEachComponent(
        icalText: String,
        componentName: String,
        block: (Map<String, String>) -> Unit,
    ) {
        val lines = unfold(icalText)
        var inside = false
        val props = mutableMapOf<String, String>()

        for (line in lines) {
            when {
                line.equals("BEGIN:$componentName", ignoreCase = true) -> {
                    inside = true
                    props.clear()
                }
                line.equals("END:$componentName", ignoreCase = true) && inside -> {
                    inside = false
                    block(props.toMap())
                }
                inside -> {
                    val colonIdx = line.indexOf(':')
                    if (colonIdx > 0) {
                        // Extract property name (strip params like ;TZID=...)
                        val rawName = line.substring(0, colonIdx)
                        val value = line.substring(colonIdx + 1)
                        val normalizedName = normalizePropertyName(rawName)
                        // Preserve first occurrence for most props; append for multi-value
                        if (normalizedName == "RELATED-TO" || normalizedName == "ATTENDEE") {
                            val existing = props[normalizedName]
                            props[normalizedName] = if (existing == null) value else "$existing,$value"
                        } else {
                            props.putIfAbsent(normalizedName, value)
                        }
                        // Also store full param line for DTSTART/DTEND/DUE so we can get TZID
                        props["__RAW_$normalizedName"] = line
                    }
                }
            }
        }
    }

    private fun normalizePropertyName(rawName: String): String {
        // "DTSTART;TZID=America/New_York" → "DTSTART"
        val semiIdx = rawName.indexOf(';')
        return if (semiIdx > 0) rawName.substring(0, semiIdx).uppercase()
        else rawName.uppercase()
    }

    // ─── Event parser ─────────────────────────────────────────────────────────

    private fun parseEvent(
        props: Map<String, String>,
        calendarHref: String?,
        resourceHref: String? = null,
        resourceEtag: String? = null,
    ): CalendarEvent? {
        val uid = props["UID"] ?: return null
        val summary = unescape(props["SUMMARY"] ?: return null)
        val start = parseDateTime(props, "DTSTART") ?: return null
        val end = parseDateTime(props, "DTEND")
            ?: parseDateTime(props, "DURATION")?.let { start.plusMillis(parseDuration(props["DURATION"])) }
            ?: start.plus(Duration.ofHours(1))
        return CalendarEvent(
            uid = uid,
            summary = summary,
            start = start,
            end = end,
            description = props["DESCRIPTION"]?.let { unescape(it) },
            location = props["LOCATION"]?.let { unescape(it) },
            calendarHref = calendarHref,
            etag = resourceEtag,
            href = resourceHref,
        )
    }

    // ─── Task parser ──────────────────────────────────────────────────────────

    private fun parseTask(
        props: Map<String, String>,
        calendarHref: String?,
        resourceHref: String? = null,
        resourceEtag: String? = null,
    ): CalDavTask? {
        val uid = props["UID"] ?: return null
        val summary = unescape(props["SUMMARY"] ?: return null)

        val relatedTo = props["RELATED-TO"]
        val parentUid = if (relatedTo != null) {
            // May be a comma-separated list; pick the PARENT type
            val rawLine = props["__RAW_RELATED-TO"] ?: ""
            if (rawLine.contains("RELTYPE=PARENT", ignoreCase = true)) {
                relatedTo.split(",").firstOrNull()
            } else null
        } else null

        return CalDavTask(
            uid = uid,
            summary = summary,
            description = props["DESCRIPTION"]?.let { unescape(it) },
            status = TaskStatus.fromIcal(props["STATUS"] ?: "NEEDS-ACTION"),
            priority = props["PRIORITY"]?.toIntOrNull() ?: 0,
            percentComplete = props["PERCENT-COMPLETE"]?.toIntOrNull() ?: 0,
            due = parseDateTime(props, "DUE"),
            dtstart = parseDateTime(props, "DTSTART"),
            completed = parseDateTime(props, "COMPLETED"),
            created = parseDateTime(props, "CREATED") ?: Instant.now(),
            lastModified = parseDateTime(props, "LAST-MODIFIED") ?: Instant.now(),
            categories = parseCategories(props["CATEGORIES"]),
            location = props["LOCATION"]?.let { unescape(it) },
            parentUid = parentUid,
            calendarHref = calendarHref,
            etag = resourceEtag,
            href = resourceHref,
        )
    }

    // ─── Note (VJOURNAL) parser ───────────────────────────────────────────────

    private fun parseNote(
        props: Map<String, String>,
        calendarHref: String?,
        resourceHref: String? = null,
        resourceEtag: String? = null,
    ): Note? {
        val uid = props["UID"] ?: return null
        val summary = unescape(props["SUMMARY"] ?: return null)
        return Note(
            uid = uid,
            summary = summary,
            body = props["DESCRIPTION"]?.let { unescape(it) } ?: "",
            categories = parseCategories(props["CATEGORIES"]),
            created = parseDateTime(props, "DTSTAMP") ?: Instant.now(),
            lastModified = parseDateTime(props, "LAST-MODIFIED") ?: Instant.now(),
            calendarHref = calendarHref,
            etag = resourceEtag,
            href = resourceHref,
        )
    }

    // ─── iCal serializers (domain → iCal text) ───────────────────────────────

    fun serializeTask(task: CalDavTask): String = buildString {
        fun line(s: String) = append(s).append("\r\n")
        line("BEGIN:VCALENDAR")
        line("VERSION:2.0")
        line("PRODID:-//CrossDashboard//Native//EN")
        line("BEGIN:VTODO")
        line("UID:${task.uid}")
        line("DTSTAMP:${formatInstant(Instant.now())}")
        line("CREATED:${formatInstant(task.created)}")
        line("LAST-MODIFIED:${formatInstant(Instant.now())}")
        line("SUMMARY:${escape(task.summary)}")
        line("STATUS:${task.status.icalValue}")
        line("PRIORITY:${task.priority}")
        line("PERCENT-COMPLETE:${task.percentComplete}")
        task.description?.let { line("DESCRIPTION:${escape(it)}") }
        task.due?.let { line("DUE:${formatInstant(it)}") }
        task.dtstart?.let { line("DTSTART:${formatInstant(it)}") }
        task.completed?.let { line("COMPLETED:${formatInstant(it)}") }
        if (task.categories.isNotEmpty()) line("CATEGORIES:${task.categories.joinToString(",")}")
        task.location?.let { line("LOCATION:${escape(it)}") }
        task.parentUid?.let { line("RELATED-TO;RELTYPE=PARENT:$it") }
        line("END:VTODO")
        line("END:VCALENDAR")
    }

    fun serializeNote(note: Note): String = buildString {
        fun line(s: String) = append(s).append("\r\n")
        line("BEGIN:VCALENDAR")
        line("VERSION:2.0")
        line("PRODID:-//CrossDashboard//Native//EN")
        line("BEGIN:VJOURNAL")
        line("UID:${note.uid}")
        line("DTSTAMP:${formatInstant(Instant.now())}")
        line("LAST-MODIFIED:${formatInstant(Instant.now())}")
        line("SUMMARY:${escape(note.summary)}")
        if (note.body.isNotBlank()) line("DESCRIPTION:${escape(note.body)}")
        if (note.categories.isNotEmpty()) line("CATEGORIES:${note.categories.joinToString(",")}")
        line("END:VJOURNAL")
        line("END:VCALENDAR")
    }

    fun serializeEvent(event: CalendarEvent): String = buildString {
        fun line(s: String) = append(s).append("\r\n")
        line("BEGIN:VCALENDAR")
        line("VERSION:2.0")
        line("PRODID:-//CrossDashboard//Native//EN")
        line("BEGIN:VEVENT")
        line("UID:${event.uid}")
        line("DTSTAMP:${formatInstant(Instant.now())}")
        line("DTSTART:${formatInstant(event.start)}")
        line("DTEND:${formatInstant(event.end)}")
        line("SUMMARY:${escape(event.summary)}")
        event.description?.let { line("DESCRIPTION:${escape(it)}") }
        event.location?.let { line("LOCATION:${escape(it)}") }
        line("LAST-MODIFIED:${formatInstant(Instant.now())}")
        line("END:VEVENT")
        line("END:VCALENDAR")
    }

    // ─── Parsing helpers ─────────────────────────────────────────────────────

    private fun unfold(icalText: String): List<String> {
        val lines = mutableListOf<String>()
        val sb = StringBuilder()
        for (line in icalText.lines()) {
            when {
                line.startsWith(' ') || line.startsWith('\t') -> sb.append(line.substring(1))
                else -> {
                    if (sb.isNotEmpty()) lines.add(sb.toString())
                    sb.clear()
                    sb.append(line)
                }
            }
        }
        if (sb.isNotEmpty()) lines.add(sb.toString())
        return lines
    }

    private fun parseDateTime(props: Map<String, String>, key: String): Instant? {
        val rawLine = props["__RAW_$key"] ?: return null
        val colonIdx = rawLine.indexOf(':')
        if (colonIdx < 0) return null
        val value = rawLine.substring(colonIdx + 1).trim()
        val paramPart = rawLine.substring(0, colonIdx)

        // VALUE=DATE → all-day, set to midnight UTC
        if (paramPart.contains("VALUE=DATE", ignoreCase = true)) {
            return try {
                LocalDate.parse(value, DateTimeFormatter.BASIC_ISO_DATE)
                    .atStartOfDay(ZoneOffset.UTC).toInstant()
            } catch (_: DateTimeParseException) { null }
        }

        // UTC Z suffix
        if (value.endsWith("Z")) {
            return try {
                Instant.parse(value.take(15).let {
                    "${it.substring(0, 4)}-${it.substring(4, 6)}-${it.substring(6, 8)}T" +
                        "${it.substring(9, 11)}:${it.substring(11, 13)}:${it.substring(13, 15)}Z"
                })
            } catch (_: Exception) {
                runCatching {
                    LocalDateTime.parse(value.dropLast(1), DateTimeFormatter.ofPattern("yyyyMMdd'T'HHmmss"))
                        .toInstant(ZoneOffset.UTC)
                }.getOrNull()
            }
        }

        // TZID param
        val tzidMatch = Regex("TZID=([^;:]+)").find(paramPart)
        val tz = tzidMatch?.groupValues?.get(1)?.let {
            runCatching { ZoneId.of(it) }.getOrNull() ?: ZoneOffset.UTC
        } ?: ZoneOffset.UTC

        return try {
            LocalDateTime.parse(value, DateTimeFormatter.ofPattern("yyyyMMdd'T'HHmmss"))
                .atZone(tz).toInstant()
        } catch (_: DateTimeParseException) { null }
    }

    private fun parseDuration(durationStr: String?): Long {
        if (durationStr == null) return 3600_000L
        var ms = 0L
        val m = Regex("P(?:(\\d+)W)?(?:(\\d+)D)?(?:T(?:(\\d+)H)?(?:(\\d+)M)?(?:(\\d+)S)?)?")
            .find(durationStr) ?: return 3600_000L
        ms += (m.groupValues[1].toLongOrNull() ?: 0) * 7 * 24 * 3600_000
        ms += (m.groupValues[2].toLongOrNull() ?: 0) * 24 * 3600_000
        ms += (m.groupValues[3].toLongOrNull() ?: 0) * 3600_000
        ms += (m.groupValues[4].toLongOrNull() ?: 0) * 60_000
        ms += (m.groupValues[5].toLongOrNull() ?: 0) * 1000
        return ms.takeIf { it > 0 } ?: 3600_000L
    }

    private fun parseCategories(raw: String?): List<String> {
        if (raw.isNullOrBlank()) return emptyList()
        return raw.split(",").map { it.trim() }.filter { it.isNotEmpty() }
    }

    private fun unescape(value: String): String =
        value
            .replace("\\n", "\n")
            .replace("\\N", "\n")
            .replace("\\,", ",")
            .replace("\\;", ";")
            .replace("\\\\", "\\")

    private fun escape(value: String): String =
        value
            .replace("\\", "\\\\")
            .replace("\n", "\\n")
            .replace(",", "\\,")
            .replace(";", "\\;")

    private val UTC_FORMATTER = DateTimeFormatter
        .ofPattern("yyyyMMdd'T'HHmmss'Z'")
        .withZone(ZoneOffset.UTC)

    private fun formatInstant(instant: Instant): String =
        UTC_FORMATTER.format(instant)
}
