package expo.modules.widget

import android.appwidget.AppWidgetManager
import android.content.ComponentName
import android.content.Context
import android.content.Intent
import androidx.work.CoroutineWorker
import androidx.work.WorkerParameters
import java.net.HttpURLConnection
import java.net.URL
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale
import java.util.TimeZone

class WidgetSyncWorker(context: Context, params: WorkerParameters) : CoroutineWorker(context, params) {

    companion object {
        const val WORK_NAME = "widget_sync_worker"
        private const val PREFS_NAME = "cross_dashboard_widget"
    }

    override suspend fun doWork(): Result {
        val prefs = applicationContext.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)

        val caldavServer = prefs.getString("worker_caldav_server", null)
        val caldavUser = prefs.getString("worker_caldav_user", null)
        val caldavPass = prefs.getString("worker_caldav_pass", null)
        val calendarHrefsRaw = prefs.getString("worker_calendar_hrefs", null)
        val calendarHrefs = if (calendarHrefsRaw.isNullOrEmpty()) emptyList()
                            else calendarHrefsRaw.split("|").filter { it.isNotBlank() }

        val giteaUrl = prefs.getString("worker_gitea_url", null)
        val giteaToken = prefs.getString("worker_gitea_token", null)
        val giteaReposRaw = prefs.getString("worker_gitea_repos", null)
        val giteaRepos = if (giteaReposRaw.isNullOrEmpty()) emptyList()
                         else giteaReposRaw.split("|").filter { it.isNotBlank() }

        val events = mutableListOf<Pair<String, Long>>()   // (formatted row, sort epoch)
        val tasks = mutableListOf<String>()
        var issueCount = 0

        // ── CalDAV ────────────────────────────────────────────────────────────
        if (caldavServer != null && caldavUser != null && caldavPass != null) {
            for (href in calendarHrefs) {
                try {
                    val fetched = fetchCalendarEvents(caldavServer, caldavUser, caldavPass, href)
                    events.addAll(fetched)
                } catch (_: Exception) {}
                try {
                    val fetched = fetchCalendarTasks(caldavServer, caldavUser, caldavPass, href)
                    tasks.addAll(fetched)
                } catch (_: Exception) {}
            }
        }

        val now = System.currentTimeMillis()
        val upcoming = events
            .filter { it.second >= now }
            .sortedBy { it.second }
            .take(3)

        // ── Gitea ─────────────────────────────────────────────────────────────
        if (giteaUrl != null && !giteaToken.isNullOrEmpty()) {
            for (repo in giteaRepos) {
                try {
                    issueCount += fetchGiteaOpenIssueCount(giteaUrl, giteaToken, repo)
                } catch (_: Exception) {}
            }
        }

        // ── Persist widget data ───────────────────────────────────────────────
        val syncTime = SimpleDateFormat("HH:mm", Locale.getDefault()).format(Date())
        val editor = prefs.edit()
        editor.putString("event_row_0", upcoming.getOrNull(0)?.first ?: "")
        editor.putString("event_row_1", upcoming.getOrNull(1)?.first ?: "")
        editor.putString("event_row_2", upcoming.getOrNull(2)?.first ?: "")
        editor.putInt("events_count", upcoming.size)
        editor.putString("task_row_0", tasks.getOrNull(0) ?: "")
        editor.putString("task_row_1", tasks.getOrNull(1) ?: "")
        editor.putString("task_row_2", tasks.getOrNull(2) ?: "")
        editor.putInt("tasks_count", tasks.size)
        editor.putInt("issues_count", issueCount)
        editor.putString("last_sync", "Synced $syncTime")
        editor.apply()

        // ── Trigger widget refresh ────────────────────────────────────────────
        val intent = Intent(applicationContext, DashboardWidgetProvider::class.java)
        intent.action = DashboardWidgetProvider.ACTION_UPDATE_WIDGET
        applicationContext.sendBroadcast(intent)

        return Result.success()
    }

    // ── CalDAV REPORT for VEVENT ──────────────────────────────────────────────
    private fun fetchCalendarEvents(
        server: String, user: String, pass: String, href: String
    ): List<Pair<String, Long>> {
        val utcFmt = SimpleDateFormat("yyyyMMdd'T'HHmmss'Z'", Locale.US).apply {
            timeZone = TimeZone.getTimeZone("UTC")
        }
        val now = Date()
        val end = Date(now.time + 30L * 24 * 60 * 60 * 1000)
        val body = """<?xml version="1.0" encoding="utf-8"?>
<C:calendar-query xmlns:D="DAV:" xmlns:C="urn:ietf:params:xml:ns:caldav">
  <D:prop><D:getetag/><C:calendar-data/></D:prop>
  <C:filter>
    <C:comp-filter name="VCALENDAR">
      <C:comp-filter name="VEVENT">
        <C:time-range start="${utcFmt.format(now)}" end="${utcFmt.format(end)}"/>
      </C:comp-filter>
    </C:comp-filter>
  </C:filter>
</C:calendar-query>"""

        val response = makeCalDavRequest(server, user, pass, href, body)
        return parseICalEvents(response)
    }

    // ── CalDAV REPORT for VTODO ───────────────────────────────────────────────
    private fun fetchCalendarTasks(
        server: String, user: String, pass: String, href: String
    ): List<String> {
        val body = """<?xml version="1.0" encoding="utf-8"?>
<C:calendar-query xmlns:D="DAV:" xmlns:C="urn:ietf:params:xml:ns:caldav">
  <D:prop><D:getetag/><C:calendar-data/></D:prop>
  <C:filter>
    <C:comp-filter name="VCALENDAR">
      <C:comp-filter name="VTODO">
        <C:prop-filter name="STATUS">
          <C:text-match collation="i;ascii-casemap" negate-condition="yes">COMPLETED</C:text-match>
        </C:prop-filter>
      </C:comp-filter>
    </C:comp-filter>
  </C:filter>
</C:calendar-query>"""

        val response = makeCalDavRequest(server, user, pass, href, body)
        return parseICalTasks(response)
    }

    private fun makeCalDavRequest(
        server: String, user: String, pass: String, href: String, body: String
    ): String {
        // Build full URL: if href starts with /, append to scheme+host portion of server
        val base = server.trimEnd('/')
        val path = if (href.startsWith("/")) href else "/$href"
        val fullUrl = if (href.startsWith("http")) href else "$base$path"

        val url = URL(fullUrl)
        val conn = url.openConnection() as HttpURLConnection
        val credentials = android.util.Base64.encodeToString(
            "$user:$pass".toByteArray(Charsets.UTF_8), android.util.Base64.NO_WRAP
        )
        conn.requestMethod = "REPORT"
        conn.setRequestProperty("Authorization", "Basic $credentials")
        conn.setRequestProperty("Content-Type", "application/xml; charset=utf-8")
        conn.setRequestProperty("Depth", "1")
        conn.connectTimeout = 12000
        conn.readTimeout = 20000
        conn.doOutput = true
        conn.outputStream.use { it.write(body.toByteArray(Charsets.UTF_8)) }

        return if (conn.responseCode in 200..299) {
            conn.inputStream.use { it.readBytes().toString(Charsets.UTF_8) }
        } else ""
    }

    // ── iCal parser: events ───────────────────────────────────────────────────
    private fun parseICalEvents(ical: String): List<Pair<String, Long>> {
        val results = mutableListOf<Pair<String, Long>>()
        val displayFmt = SimpleDateFormat("MM/dd HH:mm", Locale.getDefault())
        var inVEvent = false
        var summary = ""
        var dtstart = 0L

        for (rawLine in unfoldIcal(ical)) {
            val line = rawLine.trim()
            when {
                line == "BEGIN:VEVENT" -> { inVEvent = true; summary = ""; dtstart = 0L }
                line == "END:VEVENT" -> {
                    inVEvent = false
                    if (summary.isNotEmpty() && dtstart > 0) {
                        val label = "${displayFmt.format(Date(dtstart))} $summary"
                        results.add(Pair(label, dtstart))
                    }
                }
                inVEvent && line.startsWith("SUMMARY") -> {
                    summary = line.substringAfter(':').trim()
                }
                inVEvent && (line.startsWith("DTSTART") || line.startsWith("DTSTART;")) -> {
                    dtstart = parseICalDate(line.substringAfter(':').trim())
                }
            }
        }
        return results
    }

    // ── iCal parser: tasks ────────────────────────────────────────────────────
    private fun parseICalTasks(ical: String): List<String> {
        val results = mutableListOf<String>()
        var inVTodo = false
        var summary = ""
        var due = 0L

        for (rawLine in unfoldIcal(ical)) {
            val line = rawLine.trim()
            when {
                line == "BEGIN:VTODO" -> { inVTodo = true; summary = ""; due = 0L }
                line == "END:VTODO" -> {
                    inVTodo = false
                    if (summary.isNotEmpty()) {
                        val prefix = if (due > 0 && due < System.currentTimeMillis()) "⚠ " else "• "
                        results.add("$prefix$summary")
                    }
                }
                inVTodo && line.startsWith("SUMMARY") -> {
                    summary = line.substringAfter(':').trim()
                }
                inVTodo && (line.startsWith("DUE") || line.startsWith("DUE;")) -> {
                    due = parseICalDate(line.substringAfter(':').trim())
                }
            }
        }
        return results
    }

    // Unfold iCal continuation lines (lines starting with space/tab continue previous)
    private fun unfoldIcal(ical: String): List<String> {
        val lines = mutableListOf<String>()
        val sb = StringBuilder()
        for (line in ical.lines()) {
            if ((line.startsWith(" ") || line.startsWith("\t")) && sb.isNotEmpty()) {
                sb.append(line.trimStart())
            } else {
                if (sb.isNotEmpty()) lines.add(sb.toString())
                sb.clear()
                sb.append(line)
            }
        }
        if (sb.isNotEmpty()) lines.add(sb.toString())
        return lines
    }

    private fun parseICalDate(value: String): Long {
        return try {
            when {
                value.length == 8 -> { // DATE: 20240115
                    SimpleDateFormat("yyyyMMdd", Locale.US).parse(value)?.time ?: 0L
                }
                value.endsWith("Z") -> { // UTC datetime
                    val fmt = SimpleDateFormat("yyyyMMdd'T'HHmmss'Z'", Locale.US).apply {
                        timeZone = TimeZone.getTimeZone("UTC")
                    }
                    fmt.parse(value)?.time ?: 0L
                }
                value.length >= 15 -> { // Local datetime
                    SimpleDateFormat("yyyyMMdd'T'HHmmss", Locale.US).parse(value.take(15))?.time ?: 0L
                }
                else -> 0L
            }
        } catch (_: Exception) { 0L }
    }

    // ── Gitea open issues count ───────────────────────────────────────────────
    private fun fetchGiteaOpenIssueCount(giteaUrl: String, token: String, repo: String): Int {
        val base = giteaUrl.trimEnd('/')
        val url = URL("$base/api/v1/repos/$repo/issues?state=open&type=issues&limit=1&page=1")
        val conn = url.openConnection() as HttpURLConnection
        conn.setRequestProperty("Authorization", "token $token")
        conn.setRequestProperty("Accept", "application/json")
        conn.connectTimeout = 10000
        conn.readTimeout = 15000

        if (conn.responseCode !in 200..299) return 0

        // Gitea returns X-Total-Count header
        val total = conn.getHeaderField("X-Total-Count")
        conn.inputStream.close()
        return total?.toIntOrNull() ?: 0
    }
}
