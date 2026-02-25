package expo.modules.widget

import android.app.AlarmManager
import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import androidx.work.CoroutineWorker
import androidx.work.WorkerParameters
import java.net.HttpURLConnection
import java.net.URL
import java.text.SimpleDateFormat
import java.util.Calendar
import java.util.Date
import java.util.Locale
import java.util.TimeZone
import kotlin.math.abs

data class EventInfo(
    val formattedRow: String,
    val epochMs: Long,
    val uid: String,
    val summary: String,
    val location: String?
)

data class TaskInfo(
    val formattedRow: String,
    val dueMs: Long,
    val uid: String,
    val summary: String
)

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

        val events = mutableListOf<EventInfo>()
        val tasks = mutableListOf<TaskInfo>()
        var issueCount = 0

        // Whether this worker has credentials to actually fetch each data source.
        // If not configured, we must NOT overwrite rows — the JS side (useSyncAll)
        // owns that data and overwrites here would blank the widget every cycle.
        // Note: calendarHrefs is NOT required — if empty we fall back to the root
        // server URL, mirroring what the JS caldav.fetchEvents(undefined) does.
        val hasCaldavConfig = caldavServer != null && caldavUser != null && caldavPass != null
        val hasGiteaConfig = giteaUrl != null && !giteaToken.isNullOrEmpty() &&
                giteaRepos.isNotEmpty()

        // ── CalDAV ────────────────────────────────────────────────────────────
        if (hasCaldavConfig) {
            // Use saved calendar hrefs when available; fall back to root server URL
            // (same fallback the JS caldav service uses when no calendars are selected).
            val hrefs = if (calendarHrefs.isNotEmpty()) calendarHrefs else listOf(caldavServer!!)
            for (href in hrefs) {
                try {
                    val fetched = fetchCalendarEvents(caldavServer!!, caldavUser!!, caldavPass!!, href)
                    events.addAll(fetched)
                } catch (_: Exception) {}
                try {
                    val fetched = fetchCalendarTasks(caldavServer!!, caldavUser!!, caldavPass!!, href)
                    tasks.addAll(fetched)
                } catch (_: Exception) {}
            }
        }

        val now = System.currentTimeMillis()

        // Use start-of-today so all-day events (epochMs = midnight 00:00) are included.
        // Filtering by `epochMs >= now` would exclude them after midnight has passed.
        val startOfToday = Calendar.getInstance().apply {
            set(Calendar.HOUR_OF_DAY, 0)
            set(Calendar.MINUTE, 0)
            set(Calendar.SECOND, 0)
            set(Calendar.MILLISECOND, 0)
        }.timeInMillis
        val endOfToday = Calendar.getInstance().apply {
            set(Calendar.HOUR_OF_DAY, 23)
            set(Calendar.MINUTE, 59)
            set(Calendar.SECOND, 59)
            set(Calendar.MILLISECOND, 999)
        }.timeInMillis

        val upcoming = events
            .filter { it.epochMs >= startOfToday }
            .sortedBy { it.epochMs }
            .take(3)

        val allFutureEvents = events.filter { it.epochMs >= now }.sortedBy { it.epochMs }

        // ── Overdue tasks (DUE < now) ─────────────────────────────────────────
        val overdueTasks = tasks
            .filter { it.dueMs > 0 && it.dueMs < now }
            .take(3)

        // ── Events remaining today ────────────────────────────────────────────
        val eventsRemainingToday = events.count { it.epochMs in startOfToday..endOfToday }

        // ── Pending tasks (for old widget: sorted by due, first 3) ────────────
        val pendingRows = tasks
            .sortedWith(compareBy(nullsLast()) { it.dueMs.takeIf { ms -> ms > 0 } })
            .take(3)
            .map { it.formattedRow }

        // ── Gitea ─────────────────────────────────────────────────────────────
        if (hasGiteaConfig) {
            for (repo in giteaRepos) {
                try {
                    issueCount += fetchGiteaOpenIssueCount(giteaUrl!!, giteaToken!!, repo)
                } catch (_: Exception) {}
            }
        }

        // ── Persist widget data ───────────────────────────────────────────────
        // IMPORTANT: only overwrite a section's rows if this worker actually has
        // credentials configured for that source. Without this guard the worker
        // runs on its schedule, fetches nothing (no creds), writes empty strings
        // to SharedPrefs, and blanks every widget row — even though the JS side
        // (useSyncAll) already wrote correct data there.
        val syncTime = SimpleDateFormat("HH:mm", Locale.getDefault()).format(Date())
        val editor = prefs.edit()

        if (hasCaldavConfig) {
            // Old widget rows
            editor.putString("event_row_0", upcoming.getOrNull(0)?.formattedRow ?: "")
            editor.putString("event_row_1", upcoming.getOrNull(1)?.formattedRow ?: "")
            editor.putString("event_row_2", upcoming.getOrNull(2)?.formattedRow ?: "")
            editor.putInt("events_count", upcoming.size)
            editor.putString("task_row_0", pendingRows.getOrNull(0) ?: "")
            editor.putString("task_row_1", pendingRows.getOrNull(1) ?: "")
            editor.putString("task_row_2", pendingRows.getOrNull(2) ?: "")
            editor.putInt("tasks_count", pendingRows.size)
            // 4x4 widget rows
            editor.putString("overdue_task_row_0", overdueTasks.getOrNull(0)?.summary ?: "")
            editor.putString("overdue_task_row_1", overdueTasks.getOrNull(1)?.summary ?: "")
            editor.putString("overdue_task_row_2", overdueTasks.getOrNull(2)?.summary ?: "")
            editor.putInt("overdue_tasks_count", overdueTasks.size)
            editor.putInt("events_remaining_today", eventsRemainingToday)
        }

        if (hasGiteaConfig) {
            editor.putInt("issues_count", issueCount)
        }

        // Only stamp last_sync when at least one data source was actually queried,
        // so the widget footer reflects a real fetch, not a no-op worker run.
        if (hasCaldavConfig || hasGiteaConfig) {
            editor.putString("last_sync", "Synced $syncTime")
        }

        // Note: pomodoro_sessions_today is owned by the JS side — never overwrite here.

        editor.apply()

        // ── Schedule event and task alarms ───────────────────────────────────
        val notifEnabled = prefs.getString("notif_enabled", "false") == "true"
        val notifMinutes = prefs.getString("notif_minutes", "15")?.toIntOrNull() ?: 15
        if (notifEnabled) {
            scheduleEventAlarms(allFutureEvents, notifMinutes)
            val futureTasks = tasks.filter { it.dueMs > 0 && it.dueMs >= now }
            scheduleTaskAlarms(futureTasks, notifMinutes)
        }

        // ── Trigger widget refresh ────────────────────────────────────────────
        // Explicit broadcasts only reach the named component, so broadcast to each widget separately.
        val intent = Intent(applicationContext, DashboardWidgetProvider::class.java)
        intent.action = DashboardWidgetProvider.ACTION_UPDATE_WIDGET
        applicationContext.sendBroadcast(intent)
        val intentMain = Intent(applicationContext, MainDashboardWidgetProvider::class.java)
        intentMain.action = DashboardWidgetProvider.ACTION_UPDATE_WIDGET
        applicationContext.sendBroadcast(intentMain)

        return Result.success()
    }

    private fun scheduleEventAlarms(events: List<EventInfo>, notifMinutes: Int) {
        EventAlarmReceiver.createChannel(applicationContext)

        val alarmManager = applicationContext.getSystemService(Context.ALARM_SERVICE) as AlarmManager
        val now = System.currentTimeMillis()

        for (event in events) {
            val baseId = abs(event.uid.hashCode()) % 50_000
            val atTimeId = baseId
            val remindBeforeId = baseId + 50_000

            cancelAlarm(alarmManager, atTimeId)
            cancelAlarm(alarmManager, remindBeforeId)

            scheduleAlarm(alarmManager, event, "at_time", atTimeId, event.epochMs, notifMinutes)

            if (notifMinutes > 0) {
                val remindMs = event.epochMs - notifMinutes * 60_000L
                if (remindMs > now) {
                    scheduleAlarm(alarmManager, event, "remind_before", remindBeforeId, remindMs, notifMinutes)
                }
            }
        }
    }

    private fun scheduleAlarm(
        alarmManager: AlarmManager,
        event: EventInfo,
        type: String,
        notifId: Int,
        triggerMs: Long,
        minutes: Int
    ) {
        val alarmIntent = Intent(applicationContext, EventAlarmReceiver::class.java).apply {
            putExtra(EventAlarmReceiver.EXTRA_SUMMARY, event.summary)
            event.location?.let { putExtra(EventAlarmReceiver.EXTRA_LOCATION, it) }
            putExtra(EventAlarmReceiver.EXTRA_TYPE, type)
            putExtra(EventAlarmReceiver.EXTRA_MINUTES, minutes)
            putExtra(EventAlarmReceiver.EXTRA_NOTIF_ID, notifId)
        }
        val pendingIntent = PendingIntent.getBroadcast(
            applicationContext, notifId, alarmIntent,
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
        )
        try {
            alarmManager.setExactAndAllowWhileIdle(AlarmManager.RTC_WAKEUP, triggerMs, pendingIntent)
        } catch (_: SecurityException) {}
    }

    private fun cancelAlarm(alarmManager: AlarmManager, notifId: Int) {
        val intent = Intent(applicationContext, EventAlarmReceiver::class.java)
        val pendingIntent = PendingIntent.getBroadcast(
            applicationContext, notifId, intent,
            PendingIntent.FLAG_NO_CREATE or PendingIntent.FLAG_IMMUTABLE
        )
        if (pendingIntent != null) {
            alarmManager.cancel(pendingIntent)
            pendingIntent.cancel()
        }
    }

    // ── CalDAV REPORT for VEVENT ──────────────────────────────────────────────
    private fun fetchCalendarEvents(
        server: String, user: String, pass: String, href: String
    ): List<EventInfo> {
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
    ): List<TaskInfo> {
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
    private fun parseICalEvents(ical: String): List<EventInfo> {
        val results = mutableListOf<EventInfo>()
        val displayFmt = SimpleDateFormat("MM/dd HH:mm", Locale.getDefault())
        var inVEvent = false
        var summary = ""
        var dtstart = 0L
        var uid = ""
        var location: String? = null

        for (rawLine in unfoldIcal(ical)) {
            val line = rawLine.trim()
            when {
                line == "BEGIN:VEVENT" -> {
                    inVEvent = true; summary = ""; dtstart = 0L; uid = ""; location = null
                }
                line == "END:VEVENT" -> {
                    inVEvent = false
                    if (summary.isNotEmpty() && dtstart > 0) {
                        val label = "${displayFmt.format(Date(dtstart))} $summary"
                        val effectiveUid = uid.ifEmpty { "$summary$dtstart" }
                        results.add(EventInfo(label, dtstart, effectiveUid, summary, location))
                    }
                }
                inVEvent && line.startsWith("SUMMARY") -> {
                    summary = line.substringAfter(':').trim()
                }
                inVEvent && (line.startsWith("DTSTART") || line.startsWith("DTSTART;")) -> {
                    dtstart = parseICalDate(line.substringAfter(':').trim())
                }
                inVEvent && line.startsWith("UID:") -> {
                    uid = line.substringAfter(':').trim()
                }
                inVEvent && line.startsWith("LOCATION:") -> {
                    location = line.substringAfter(':').trim().ifEmpty { null }
                }
            }
        }
        return results
    }

    // ── iCal parser: tasks ────────────────────────────────────────────────────
    private fun parseICalTasks(ical: String): List<TaskInfo> {
        val results = mutableListOf<TaskInfo>()
        var inVTodo = false
        var summary = ""
        var due = 0L
        var uid = ""
        val now = System.currentTimeMillis()

        for (rawLine in unfoldIcal(ical)) {
            val line = rawLine.trim()
            when {
                line == "BEGIN:VTODO" -> { inVTodo = true; summary = ""; due = 0L; uid = "" }
                line == "END:VTODO" -> {
                    inVTodo = false
                    if (summary.isNotEmpty()) {
                        val prefix = if (due > 0 && due < now) "⚠ " else "• "
                        val formattedRow = "$prefix$summary"
                        val effectiveUid = uid.ifEmpty { "$summary$due" }
                        results.add(TaskInfo(formattedRow, due, effectiveUid, summary))
                    }
                }
                inVTodo && line.startsWith("SUMMARY") -> {
                    summary = line.substringAfter(':').trim()
                }
                inVTodo && (line.startsWith("DUE:") || line.startsWith("DUE;")) -> {
                    due = parseICalDate(line.substringAfter(':').trim())
                }
                inVTodo && line.startsWith("UID:") -> {
                    uid = line.substringAfter(':').trim()
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
                value.length == 8 -> {
                    SimpleDateFormat("yyyyMMdd", Locale.US).parse(value)?.time ?: 0L
                }
                value.endsWith("Z") -> {
                    val fmt = SimpleDateFormat("yyyyMMdd'T'HHmmss'Z'", Locale.US).apply {
                        timeZone = TimeZone.getTimeZone("UTC")
                    }
                    fmt.parse(value)?.time ?: 0L
                }
                value.length >= 15 -> {
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

        val total = conn.getHeaderField("X-Total-Count")
        conn.inputStream.close()
        return total?.toIntOrNull() ?: 0
    }
}
