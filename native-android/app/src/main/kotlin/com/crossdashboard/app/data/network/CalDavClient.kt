package com.crossdashboard.app.data.network

import com.crossdashboard.app.data.parser.ICalParser
import com.crossdashboard.app.data.prefs.CredentialKey
import com.crossdashboard.app.data.prefs.SecureStore
import com.crossdashboard.app.domain.model.*
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import okhttp3.*
import okhttp3.MediaType.Companion.toMediaType
import okhttp3.RequestBody.Companion.toRequestBody
import java.io.IOException
import java.time.Instant
import java.time.ZoneOffset
import java.time.format.DateTimeFormatter
import java.util.UUID
import javax.inject.Inject
import javax.inject.Singleton

/**
 * CalDAV HTTP client using OkHttp.
 *
 * Uses Basic authentication from SecureStore. Supports the full set of CalDAV
 * methods needed: PROPFIND (calendar discovery), REPORT (bulk fetch), PUT (create/update),
 * DELETE (delete), and MKCALENDAR.
 *
 * Credential loading is lazy — reads from SecureStore at call time so Settings
 * changes take effect without restarting the service.
 */
@Singleton
class CalDavClient @Inject constructor(
    private val secureStore: SecureStore,
    private val httpClient: OkHttpClient,
) {

    // ─── Calendar discovery (PROPFIND) ────────────────────────────────────────

    suspend fun fetchCalendars(): List<CalDavCalendar> = withContext(Dispatchers.IO) {
        val base = caldavBase() ?: return@withContext emptyList()
        val body = """
            <?xml version="1.0" encoding="utf-8"?>
            <d:propfind xmlns:d="DAV:" xmlns:c="urn:ietf:params:xml:ns:caldav"
                        xmlns:a="http://apple.com/ns/ical/">
              <d:prop>
                <d:displayname/>
                <d:resourcetype/>
                <c:calendar-description/>
                <c:supported-calendar-component-set/>
                <a:calendar-color/>
                <d:getctag xmlns:d="http://calendarserver.org/ns/"/>
              </d:prop>
            </d:propfind>
        """.trimIndent()

        val response = execute(
            method = "PROPFIND",
            url = base,
            bodyString = body,
            extraHeaders = mapOf("Depth" to "1", "Content-Type" to "application/xml"),
        ) ?: return@withContext emptyList()

        parseCalendarsFromPropfind(response, base)
    }

    // ─── Event fetching (REPORT) ──────────────────────────────────────────────

    suspend fun fetchEvents(
        calendarHrefs: List<String>,
        from: Instant,
        to: Instant,
    ): List<CalendarEvent> = withContext(Dispatchers.IO) {
        val server = serverUrl() ?: return@withContext emptyList()
        val results = mutableListOf<CalendarEvent>()

        for (href in calendarHrefs) {
            val url = if (href.startsWith("http")) href else "$server$href"
            val report = calendarQueryReport(from, to, "VEVENT")
            val response = execute("REPORT", url, report, mapOf("Depth" to "1", "Content-Type" to "application/xml"))
                ?: continue
            extractCalendarResources(response).forEach { resource ->
                val absHref = resource.href?.let { if (it.startsWith("http")) it else "$server$it" }
                results.addAll(ICalParser.parseEvents(resource.icalData, href, absHref, resource.etag))
            }
        }
        results
    }

    // ─── Task fetching (REPORT) ───────────────────────────────────────────────

    suspend fun fetchTasks(calendarHrefs: List<String>): List<CalDavTask> = withContext(Dispatchers.IO) {
        val server = serverUrl() ?: return@withContext emptyList()
        val results = mutableListOf<CalDavTask>()

        for (href in calendarHrefs) {
            val url = if (href.startsWith("http")) href else "$server$href"
            val report = todoQueryReport()
            val response = execute("REPORT", url, report, mapOf("Depth" to "1", "Content-Type" to "application/xml"))
                ?: continue
            extractCalendarResources(response).forEach { resource ->
                val absHref = resource.href?.let { if (it.startsWith("http")) it else "$server$it" }
                results.addAll(ICalParser.parseTasks(resource.icalData, href, absHref, resource.etag))
            }
        }
        results
    }

    // ─── Note fetching (REPORT) ───────────────────────────────────────────────

    suspend fun fetchNotes(calendarHrefs: List<String>): List<Note> = withContext(Dispatchers.IO) {
        val server = serverUrl() ?: return@withContext emptyList()
        val results = mutableListOf<Note>()

        for (href in calendarHrefs) {
            val url = if (href.startsWith("http")) href else "$server$href"
            val report = calendarQueryReport(componentType = "VJOURNAL")
            val response = execute("REPORT", url, report, mapOf("Depth" to "1", "Content-Type" to "application/xml"))
                ?: continue
            extractCalendarResources(response).forEach { resource ->
                val absHref = resource.href?.let { if (it.startsWith("http")) it else "$server$it" }
                results.addAll(ICalParser.parseNotes(resource.icalData, href, absHref, resource.etag))
            }
        }
        results
    }

    // ─── Task CRUD ────────────────────────────────────────────────────────────

    suspend fun createTask(task: CalDavTask, calendarHref: String): CalDavTask = withContext(Dispatchers.IO) {
        val server = serverUrl() ?: throw IOException("No server configured")
        val base = if (calendarHref.startsWith("http")) calendarHref else "$server$calendarHref"
        val uid = task.uid.ifBlank { UUID.randomUUID().toString() }
        val resourceUrl = "$base${uid}.ics"
        val ical = ICalParser.serializeTask(task.copy(uid = uid))

        put(resourceUrl, ical, task.etag)
        task.copy(uid = uid, href = resourceUrl)
    }

    suspend fun updateTask(task: CalDavTask): Unit = withContext(Dispatchers.IO) {
        val server = serverUrl() ?: throw IOException("No server configured")
        val url = task.href
            ?: task.calendarHref?.let { href ->
                val base = if (href.startsWith("http")) href else "$server$href"
                "$base${task.uid}.ics"
            }
            ?: throw IOException("No href for task ${task.uid}")

        val ical = ICalParser.serializeTask(task)
        // Unconditional PUT — no If-Match — avoids
        // 412 Precondition Failed when the cached ETag is stale.
        put(url, ical, etag = null)
    }

    suspend fun deleteTask(task: CalDavTask): Unit = withContext(Dispatchers.IO) {
        val server = serverUrl() ?: throw IOException("No server configured")
        val url = task.href
            ?: task.calendarHref?.let { href ->
                val base = if (href.startsWith("http")) href else "$server$href"
                "$base${task.uid}.ics"
            }
            ?: throw IOException("No href for task ${task.uid}")

        delete(url, task.etag)
    }

    // ─── Note CRUD ────────────────────────────────────────────────────────────

    suspend fun createNote(note: Note, calendarHref: String): Note = withContext(Dispatchers.IO) {
        val server = serverUrl() ?: throw IOException("No server configured")
        val base = if (calendarHref.startsWith("http")) calendarHref else "$server$calendarHref"
        val uid = note.uid.ifBlank { UUID.randomUUID().toString() }
        val resourceUrl = "$base${uid}.ics"
        put(resourceUrl, ICalParser.serializeNote(note.copy(uid = uid)), null)
        note.copy(uid = uid, href = resourceUrl)
    }

    suspend fun updateNote(note: Note): Unit = withContext(Dispatchers.IO) {
        val url = resolveHref(note.href, note.uid, note.calendarHref)
        put(url, ICalParser.serializeNote(note), etag = null)
    }

    suspend fun deleteNote(note: Note): Unit = withContext(Dispatchers.IO) {
        val url = resolveHref(note.href, note.uid, note.calendarHref)
        delete(url, note.etag)
    }

    // ─── Event CRUD ───────────────────────────────────────────────────────────

    suspend fun createEvent(event: CalendarEvent, calendarHref: String): CalendarEvent = withContext(Dispatchers.IO) {
        val server = serverUrl() ?: throw IOException("No server configured")
        val base = if (calendarHref.startsWith("http")) calendarHref else "$server$calendarHref"
        val uid = event.uid.ifBlank { UUID.randomUUID().toString() }
        val resourceUrl = "$base${uid}.ics"
        put(resourceUrl, ICalParser.serializeEvent(event.copy(uid = uid)), null)
        event.copy(uid = uid, href = resourceUrl)
    }

    suspend fun deleteEvent(event: CalendarEvent): Unit = withContext(Dispatchers.IO) {
        val url = resolveHref(event.href, event.uid, event.calendarHref)
        delete(url, event.etag)
    }

    // ─── Connection test ──────────────────────────────────────────────────────

    suspend fun testConnection(): Result<String> = withContext(Dispatchers.IO) {
        try {
            val creds = loadCredentials() ?: return@withContext Result.failure(IOException("No credentials"))
            val request = Request.Builder()
                .url(creds.serverUrl)
                .header("Authorization", Credentials.basic(creds.username, creds.password ?: ""))
                .head()
                .build()
            val response = httpClient.newCall(request).execute()
            if (response.isSuccessful || response.code == 207 || response.code == 401) {
                Result.success(response.message)
            } else {
                Result.failure(IOException("HTTP ${response.code}"))
            }
        } catch (e: Exception) {
            Result.failure(e)
        }
    }

    // ─── HTTP primitives ─────────────────────────────────────────────────────

    private fun put(url: String, icalText: String, etag: String?) {
        val body = icalText.toRequestBody("text/calendar; charset=utf-8".toMediaType())
        val builder = Request.Builder()
            .url(url)
            .header("Content-Type", "text/calendar; charset=utf-8")
            .put(body)
        etag?.let { builder.header("If-Match", it) }
        addAuth(builder)
        httpClient.newCall(builder.build()).execute().use { r ->
            if (!r.isSuccessful && r.code != 201 && r.code != 204) {
                val errorBody = runCatching { r.body?.string() }.getOrNull()?.take(500)
                throw IOException("PUT failed: HTTP ${r.code} — $errorBody")
            }
        }
    }

    private fun delete(url: String, etag: String?) {
        val builder = Request.Builder()
            .url(url)
            .delete()
        etag?.let { builder.header("If-Match", it) }
        addAuth(builder)
        httpClient.newCall(builder.build()).execute().use { r ->
            if (!r.isSuccessful && r.code != 204) throw IOException("DELETE failed: HTTP ${r.code}")
        }
    }

    private fun execute(
        method: String,
        url: String,
        bodyString: String? = null,
        extraHeaders: Map<String, String> = emptyMap(),
    ): String? {
        val requestBody = bodyString?.toRequestBody("application/xml; charset=utf-8".toMediaType())
        val builder = Request.Builder()
            .url(url)
            .method(method, requestBody)
        extraHeaders.forEach { (k, v) -> builder.header(k, v) }
        addAuth(builder)
        return try {
            httpClient.newCall(builder.build()).execute().use { r ->
                if (r.isSuccessful || r.code == 207) r.body?.string()
                else null
            }
        } catch (_: Exception) { null }
    }

    private fun addAuth(builder: Request.Builder) {
        val creds = loadCredentials() ?: return
        builder.header("Authorization", Credentials.basic(creds.username, creds.password ?: ""))
    }

    // ─── Credential helpers ───────────────────────────────────────────────────

    private fun loadCredentials(): CalDavCredentials? {
        val server = secureStore.get(CredentialKey.CALDAV_SERVER) ?: return null
        val username = secureStore.get(CredentialKey.CALDAV_USERNAME) ?: return null
        val password = secureStore.get(CredentialKey.CALDAV_PASSWORD)
        val method = secureStore.get(CredentialKey.CALDAV_AUTH_METHOD)
            ?.let { runCatching { CalDavAuthMethod.valueOf(it) }.getOrNull() }
            ?: CalDavAuthMethod.MANUAL
        return CalDavCredentials(method, server, username, password, null)
    }

    private fun serverUrl(): String? = secureStore.get(CredentialKey.CALDAV_SERVER)
        ?.trimEnd('/')

    private fun caldavBase(): String? {
        val server = serverUrl() ?: return null
        val username = secureStore.get(CredentialKey.CALDAV_USERNAME) ?: return null
        // Standard Nextcloud/Radicale CalDAV path
        return if (server.contains("/dav/calendars")) server
        else "$server/remote.php/dav/calendars/$username/"
    }

    private fun resolveHref(href: String?, uid: String, calendarHref: String?): String {
        if (href != null) return href
        val server = serverUrl() ?: throw IOException("No server configured")
        val base = calendarHref?.let {
            if (it.startsWith("http")) it else "$server$it"
        } ?: throw IOException("No href for $uid")
        return "$base$uid.ics"
    }

    // ─── XML response parsers ─────────────────────────────────────────────────

    private fun parseCalendarsFromPropfind(xml: String, baseUrl: String): List<CalDavCalendar> {
        val calendars = mutableListOf<CalDavCalendar>()
        // Simple regex-based extraction — no full XML parser dependency needed
        val responseBlocks = Regex("""<[^:]*:?response\b[^>]*>(.*?)</[^:]*:?response>""", RegexOption.DOT_MATCHES_ALL)
            .findAll(xml)

        for (block in responseBlocks) {
            val content = block.groupValues[1]
            val href = extractXmlValue(content, "href") ?: continue

            // Only include calendar collections (not address books etc)
            val isCalendar = content.contains("calendar", ignoreCase = true) &&
                !content.contains("addressbook", ignoreCase = true)
            if (!isCalendar) continue

            val displayName = extractXmlValue(content, "displayname") ?: href.trimEnd('/').substringAfterLast('/')
            val color = extractXmlValue(content, "calendar-color")
                ?: extractXmlValue(content, "cal:calendar-color")
                ?: extractXmlValue(content, "a:calendar-color")

            // Parse supported component types
            val components = Regex("""<[^:]*:?comp\s+name="([^"]+)"""")
                .findAll(content)
                .map { it.groupValues[1].uppercase() }
                .toList()

            val server = baseUrl.let {
                runCatching { java.net.URL(it) }.getOrNull()?.let { u ->
                    "${u.protocol}://${u.host}${if (u.port != -1) ":${u.port}" else ""}"
                }
            } ?: ""

            calendars.add(
                CalDavCalendar(
                    href = href,
                    displayName = displayName,
                    color = normalizeColor(color),
                    components = components.ifEmpty { listOf("VEVENT") },
                )
            )
        }
        return calendars
    }

    private data class CalendarResource(
        val href: String?,
        val etag: String?,
        val icalData: String,
    )

    private fun extractCalendarResources(multiStatusXml: String): List<CalendarResource> {
        val resources = mutableListOf<CalendarResource>()
        Regex("""<[^:]*:?response\b[^>]*>(.*?)</[^:]*:?response>""", RegexOption.DOT_MATCHES_ALL)
            .findAll(multiStatusXml)
            .forEach { block ->
                val content = block.groupValues[1]
                val icalData = Regex(
                    """<[^:]*:?calendar-data[^>]*>(.*?)</[^:]*:?calendar-data>""",
                    RegexOption.DOT_MATCHES_ALL,
                ).find(content)?.groupValues?.get(1)?.trim() ?: return@forEach
                if (icalData.isEmpty()) return@forEach
                val href = extractXmlValue(content, "href")
                // Strip surrounding quotes that some servers include in ETag values
                val etag = extractXmlValue(content, "getetag")?.trim('"')
                resources.add(CalendarResource(href, etag, icalData))
            }
        return resources
    }

    private fun extractXmlValue(xml: String, tag: String): String? {
        val pattern = Regex("""<[^:]*:?$tag[^>]*>(.*?)</[^:]*:?$tag>""", RegexOption.DOT_MATCHES_ALL)
        return pattern.find(xml)?.groupValues?.get(1)?.trim()?.takeIf { it.isNotEmpty() }
    }

    private fun normalizeColor(raw: String?): String? {
        if (raw == null) return null
        val hex = raw.removePrefix("#").trim()
        return if (hex.length >= 6) "#${hex.take(6).uppercase()}" else null
    }

    // ─── REPORT request bodies ────────────────────────────────────────────────

    private val ISO = DateTimeFormatter.ofPattern("yyyyMMdd'T'HHmmss'Z'").withZone(ZoneOffset.UTC)

    private fun calendarQueryReport(
        from: Instant? = null,
        to: Instant? = null,
        componentType: String = "VEVENT",
    ): String {
        val timeFilter = if (from != null && to != null) {
            """<c:time-range start="${ISO.format(from)}" end="${ISO.format(to)}"/>"""
        } else ""

        return """
            <?xml version="1.0" encoding="utf-8"?>
            <c:calendar-query xmlns:d="DAV:" xmlns:c="urn:ietf:params:xml:ns:caldav">
              <d:prop>
                <d:getetag/>
                <c:calendar-data/>
              </d:prop>
              <c:filter>
                <c:comp-filter name="VCALENDAR">
                  <c:comp-filter name="$componentType">
                    $timeFilter
                  </c:comp-filter>
                </c:comp-filter>
              </c:filter>
            </c:calendar-query>
        """.trimIndent()
    }

    private fun todoQueryReport(): String = """
        <?xml version="1.0" encoding="utf-8"?>
        <c:calendar-query xmlns:d="DAV:" xmlns:c="urn:ietf:params:xml:ns:caldav">
          <d:prop>
            <d:getetag/>
            <c:calendar-data/>
          </d:prop>
          <c:filter>
            <c:comp-filter name="VCALENDAR">
              <c:comp-filter name="VTODO"/>
            </c:comp-filter>
          </c:filter>
        </c:calendar-query>
    """.trimIndent()
}
