import Foundation
import CrossDashboardKit

/// CalDAV HTTP client using URLSession.
///
/// Uses Basic auth loaded lazily from KeychainStore. Supports all CalDAV methods
/// needed: PROPFIND (calendar discovery), REPORT (bulk fetch), PUT (create/update),
/// DELETE (delete). URLRequest.httpMethod accepts arbitrary strings so non-standard
/// methods work without any third-party library.
///
/// Direct Swift port of CalDavClient.kt.
final class CalDavClient: Sendable {

    private let keychain: KeychainStore
    private let session: URLSession

    init(keychain: KeychainStore = .shared) {
        self.keychain = keychain
        let config = URLSessionConfiguration.default
        config.timeoutIntervalForRequest = 30
        config.timeoutIntervalForResource = 60
        self.session = URLSession(configuration: config)
    }

    // ─── Calendar discovery (PROPFIND) ────────────────────────────────────────

    func fetchCalendars() async -> [CalDavCalendar] {
        guard let base = caldavBase() else { return [] }
        let body = """
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
            """
        guard let xml = await execute(
            method: "PROPFIND",
            url: base,
            body: body,
            headers: ["Depth": "1", "Content-Type": "application/xml"]
        ) else { return [] }
        return parseCalendarsFromPropfind(xml, baseUrl: base)
    }

    // ─── Event fetching (REPORT) ──────────────────────────────────────────────

    func fetchEvents(
        calendarHrefs: [String],
        from: Date,
        to: Date
    ) async -> [CalendarEvent]? {
        guard let server = serverUrl() else { return nil }
        var results: [CalendarEvent] = []
        for href in calendarHrefs {
            let url = href.hasPrefix("http") ? href : "\(server)\(href)"
            let report = calendarQueryReport(from: from, to: to, componentType: "VEVENT")
            guard let xml = await execute(
                method: "REPORT",
                url: url,
                body: report,
                headers: ["Depth": "1", "Content-Type": "application/xml"]
            ) else { return nil }
            for resource in extractCalendarResources(xml) {
                let absHref = resource.href.map { $0.hasPrefix("http") ? $0 : "\(server)\($0)" }
                results += ICalParser.parseEvents(icalText: resource.icalData, calendarHref: href, resourceHref: absHref, resourceEtag: resource.etag)
            }
        }
        return results
    }

    // ─── Task fetching (REPORT) ───────────────────────────────────────────────

    func fetchTasks(calendarHrefs: [String]) async -> [CalDavTask]? {
        guard let server = serverUrl() else { return nil }
        var results: [CalDavTask] = []
        for href in calendarHrefs {
            let url = href.hasPrefix("http") ? href : "\(server)\(href)"
            let report = todoQueryReport()
            guard let xml = await execute(
                method: "REPORT",
                url: url,
                body: report,
                headers: ["Depth": "1", "Content-Type": "application/xml"]
            ) else { return nil }
            for resource in extractCalendarResources(xml) {
                let absHref = resource.href.map { $0.hasPrefix("http") ? $0 : "\(server)\($0)" }
                results += ICalParser.parseTasks(icalText: resource.icalData, calendarHref: href, resourceHref: absHref, resourceEtag: resource.etag)
            }
        }
        return results
    }

    // ─── Note fetching (REPORT) ───────────────────────────────────────────────

    func fetchNotes(calendarHrefs: [String]) async -> [Note]? {
        guard let server = serverUrl() else { return nil }
        var results: [Note] = []
        for href in calendarHrefs {
            let url = href.hasPrefix("http") ? href : "\(server)\(href)"
            let report = calendarQueryReport(componentType: "VJOURNAL")
            guard let xml = await execute(
                method: "REPORT",
                url: url,
                body: report,
                headers: ["Depth": "1", "Content-Type": "application/xml"]
            ) else { return nil }
            for resource in extractCalendarResources(xml) {
                let absHref = resource.href.map { $0.hasPrefix("http") ? $0 : "\(server)\($0)" }
                results += ICalParser.parseNotes(icalText: resource.icalData, calendarHref: href, resourceHref: absHref, resourceEtag: resource.etag)
            }
        }
        return results
    }

    // ─── Task CRUD ────────────────────────────────────────────────────────────

    func createTask(_ task: CalDavTask, calendarHref: String) async throws -> CalDavTask {
        guard let server = serverUrl() else { throw CalDavError.noCredentials }
        let base = calendarHref.hasPrefix("http") ? calendarHref : "\(server)\(calendarHref)"
        let uid = task.uid.isEmpty ? UUID().uuidString : task.uid
        let resourceUrl = "\(base)\(uid).ics"
        let ical = ICalParser.serializeTask(task)
        try await put(url: resourceUrl, icalText: ical, etag: task.etag)
        return CalDavTask(
            uid: uid, summary: task.summary, description: task.description,
            status: task.status, priority: task.priority, percentComplete: task.percentComplete,
            due: task.due, dtstart: task.dtstart, completed: task.completed,
            created: task.created, lastModified: task.lastModified,
            categories: task.categories, location: task.location, parentUid: task.parentUid,
            calendarHref: task.calendarHref, etag: task.etag, href: resourceUrl
        )
    }

    func updateTask(_ task: CalDavTask) async throws {
        let url = try resolveHref(href: task.href, uid: task.uid, calendarHref: task.calendarHref)
        let ical = ICalParser.serializeTask(task)
        // Unconditional PUT — no If-Match — avoids 412 Precondition Failed on stale ETag
        try await put(url: url, icalText: ical, etag: nil)
    }

    func deleteTask(_ task: CalDavTask) async throws {
        let url = try resolveHref(href: task.href, uid: task.uid, calendarHref: task.calendarHref)
        try await delete(url: url, etag: task.etag)
    }

    // ─── Note CRUD ────────────────────────────────────────────────────────────

    func createNote(_ note: Note, calendarHref: String) async throws -> Note {
        guard let server = serverUrl() else { throw CalDavError.noCredentials }
        let base = calendarHref.hasPrefix("http") ? calendarHref : "\(server)\(calendarHref)"
        let uid = note.uid.isEmpty ? UUID().uuidString : note.uid
        let resourceUrl = "\(base)\(uid).ics"
        try await put(url: resourceUrl, icalText: ICalParser.serializeNote(note), etag: nil)
        return Note(uid: uid, summary: note.summary, body: note.body, categories: note.categories,
                    created: note.created, lastModified: note.lastModified,
                    calendarHref: note.calendarHref, etag: note.etag, href: resourceUrl)
    }

    func updateNote(_ note: Note) async throws {
        let url = try resolveHref(href: note.href, uid: note.uid, calendarHref: note.calendarHref)
        try await put(url: url, icalText: ICalParser.serializeNote(note), etag: nil)
    }

    func deleteNote(_ note: Note) async throws {
        let url = try resolveHref(href: note.href, uid: note.uid, calendarHref: note.calendarHref)
        try await delete(url: url, etag: note.etag)
    }

    // ─── Event CRUD ───────────────────────────────────────────────────────────

    func createEvent(_ event: CalendarEvent, calendarHref: String) async throws -> CalendarEvent {
        guard let server = serverUrl() else { throw CalDavError.noCredentials }
        let base = calendarHref.hasPrefix("http") ? calendarHref : "\(server)\(calendarHref)"
        let uid = event.uid.isEmpty ? UUID().uuidString : event.uid
        let resourceUrl = "\(base)\(uid).ics"
        try await put(url: resourceUrl, icalText: ICalParser.serializeEvent(event), etag: nil)
        return CalendarEvent(uid: uid, summary: event.summary, start: event.start, end: event.end,
                             description: event.description, location: event.location,
                             calendarHref: event.calendarHref, etag: event.etag, href: resourceUrl)
    }

    func deleteEvent(_ event: CalendarEvent) async throws {
        let url = try resolveHref(href: event.href, uid: event.uid, calendarHref: event.calendarHref)
        try await delete(url: url, etag: event.etag)
    }

    func updateEvent(_ event: CalendarEvent) async throws {
        let url = try resolveHref(href: event.href, uid: event.uid, calendarHref: event.calendarHref)
        try await put(url: url, icalText: ICalParser.serializeEvent(event), etag: nil)
    }

    // ─── Connection test ──────────────────────────────────────────────────────

    func testConnection() async -> Result<String, Error> {
        guard let creds = loadCredentials() else {
            return .failure(CalDavError.noCredentials)
        }
        guard let url = URL(string: creds.serverUrl) else {
            return .failure(CalDavError.invalidUrl(creds.serverUrl))
        }
        var request = URLRequest(url: url)
        request.httpMethod = "HEAD"
        addAuth(&request)
        do {
            let (_, response) = try await session.data(for: request)
            if let http = response as? HTTPURLResponse {
                let code = http.statusCode
                if code == 200 || code == 207 || code == 401 {
                    return .success(HTTPURLResponse.localizedString(forStatusCode: code))
                }
                return .failure(CalDavError.httpError(code))
            }
            return .success("OK")
        } catch {
            return .failure(error)
        }
    }

    // ─── HTTP primitives ─────────────────────────────────────────────────────

    private func put(url: String, icalText: String, etag: String?) async throws {
        guard let reqUrl = URL(string: url) else { throw CalDavError.invalidUrl(url) }
        var request = URLRequest(url: reqUrl)
        request.httpMethod = "PUT"
        request.setValue("text/calendar; charset=utf-8", forHTTPHeaderField: "Content-Type")
        if let etag { request.setValue(etag, forHTTPHeaderField: "If-Match") }
        request.httpBody = icalText.data(using: .utf8)
        addAuth(&request)
        let (_, response) = try await session.data(for: request)
        if let http = response as? HTTPURLResponse {
            let code = http.statusCode
            if code != 200 && code != 201 && code != 204 {
                throw CalDavError.httpError(code)
            }
        }
    }

    private func delete(url: String, etag: String?) async throws {
        guard let reqUrl = URL(string: url) else { throw CalDavError.invalidUrl(url) }
        var request = URLRequest(url: reqUrl)
        request.httpMethod = "DELETE"
        if let etag { request.setValue(etag, forHTTPHeaderField: "If-Match") }
        addAuth(&request)
        let (_, response) = try await session.data(for: request)
        if let http = response as? HTTPURLResponse, http.statusCode != 200 && http.statusCode != 204 {
            throw CalDavError.httpError(http.statusCode)
        }
    }

    private func execute(
        method: String,
        url: String,
        body: String? = nil,
        headers: [String: String] = [:]
    ) async -> String? {
        guard let reqUrl = URL(string: url) else { return nil }
        var request = URLRequest(url: reqUrl)
        request.httpMethod = method
        headers.forEach { request.setValue($1, forHTTPHeaderField: $0) }
        if let body { request.httpBody = body.data(using: .utf8) }
        addAuth(&request)
        do {
            let (data, response) = try await session.data(for: request)
            if let http = response as? HTTPURLResponse,
               http.statusCode == 200 || http.statusCode == 207 {
                return String(data: data, encoding: .utf8)
            }
        } catch {}
        return nil
    }

    // ─── Auth ─────────────────────────────────────────────────────────────────

    private func addAuth(_ request: inout URLRequest) {
        guard let creds = loadCredentials(),
              let data = "\(creds.username):\(creds.password ?? "")".data(using: .utf8)
        else { return }
        request.setValue("Basic \(data.base64EncodedString())", forHTTPHeaderField: "Authorization")
    }

    private func loadCredentials() -> CalDavCredentials? {
        guard let server   = keychain.get(CredentialKey.caldavServer),
              let username = keychain.get(CredentialKey.caldavUsername)
        else { return nil }
        let password = keychain.get(CredentialKey.caldavPassword)
        let method   = keychain.get(CredentialKey.caldavAuthMethod)
            .flatMap { CalDavAuthMethod(rawValue: $0) } ?? .manual
        return CalDavCredentials(authMethod: method, serverUrl: server, username: username, password: password)
    }

    private func serverUrl() -> String? {
        keychain.get(CredentialKey.caldavServer).map { $0.trimmingCharacters(in: CharacterSet(charactersIn: "/")) }
    }

    private func caldavBase() -> String? {
        guard let server   = serverUrl(),
              let username = keychain.get(CredentialKey.caldavUsername)
        else { return nil }
        if server.contains("/dav/calendars") { return server }
        return "\(server)/remote.php/dav/calendars/\(username)/"
    }

    private func resolveHref(href: String?, uid: String, calendarHref: String?) throws -> String {
        if let href { return href }
        guard let server = serverUrl() else { throw CalDavError.noCredentials }
        guard let calHref = calendarHref else { throw CalDavError.missingHref(uid) }
        let base = calHref.hasPrefix("http") ? calHref : "\(server)\(calHref)"
        return "\(base)\(uid).ics"
    }

    // ─── XML parsers ─────────────────────────────────────────────────────────

    private func parseCalendarsFromPropfind(_ xml: String, baseUrl: String) -> [CalDavCalendar] {
        var calendars: [CalDavCalendar] = []
        let responsePattern = try! NSRegularExpression(
            pattern: "<[^:]*:?response\\b[^>]*>(.*?)</[^:]*:?response>",
            options: [.dotMatchesLineSeparators]
        )
        let blocks = responsePattern.matches(in: xml, range: NSRange(xml.startIndex..., in: xml))
        for block in blocks {
            guard let range = Range(block.range(at: 1), in: xml) else { continue }
            let content = String(xml[range])

            guard content.lowercased().contains("calendar"),
                  !content.lowercased().contains("addressbook")
            else { continue }

            guard let href = extractXmlValue(content, tag: "href") else { continue }
            let displayName = extractXmlValue(content, tag: "displayname")
                ?? href.trimmingCharacters(in: CharacterSet(charactersIn: "/")).components(separatedBy: "/").last
                ?? href

            let colorRaw = extractXmlValue(content, tag: "calendar-color")
                ?? extractXmlValue(content, tag: "cal:calendar-color")
                ?? extractXmlValue(content, tag: "a:calendar-color")

            let compPattern = try! NSRegularExpression(pattern: #"<[^:]*:?comp\s+name="([^"]+)""#)
            let compMatches = compPattern.matches(in: content, range: NSRange(content.startIndex..., in: content))
            let components = compMatches.compactMap { m -> String? in
                guard let r = Range(m.range(at: 1), in: content) else { return nil }
                return String(content[r]).uppercased()
            }

            calendars.append(CalDavCalendar(
                href: href,
                displayName: displayName,
                color: normalizeColor(colorRaw),
                ctag: nil,
                components: components.isEmpty ? ["VEVENT"] : components
            ))
        }
        return calendars
    }

    private struct CalendarResource {
        let href: String?
        let etag: String?
        let icalData: String
    }

    private func extractCalendarResources(_ xml: String) -> [CalendarResource] {
        var resources: [CalendarResource] = []
        let responsePattern = try! NSRegularExpression(
            pattern: "<[^:]*:?response\\b[^>]*>(.*?)</[^:]*:?response>",
            options: [.dotMatchesLineSeparators]
        )
        let calDataPattern = try! NSRegularExpression(
            pattern: "<[^:]*:?calendar-data[^>]*>(.*?)</[^:]*:?calendar-data>",
            options: [.dotMatchesLineSeparators]
        )
        let blocks = responsePattern.matches(in: xml, range: NSRange(xml.startIndex..., in: xml))
        for block in blocks {
            guard let range = Range(block.range(at: 1), in: xml) else { continue }
            let content = String(xml[range])
            guard let m = calDataPattern.firstMatch(in: content, range: NSRange(content.startIndex..., in: content)),
                  let r = Range(m.range(at: 1), in: content)
            else { continue }
            let icalData = String(content[r]).trimmingCharacters(in: .whitespacesAndNewlines)
            if icalData.isEmpty { continue }
            let href = extractXmlValue(content, tag: "href")
            let etag = extractXmlValue(content, tag: "getetag")?.trimmingCharacters(in: CharacterSet(charactersIn: "\""))
            resources.append(CalendarResource(href: href, etag: etag, icalData: icalData))
        }
        return resources
    }

    private func extractXmlValue(_ xml: String, tag: String) -> String? {
        guard let pattern = try? NSRegularExpression(
            pattern: "<[^:]*:?\(NSRegularExpression.escapedPattern(for: tag))[^>]*>(.*?)</[^:]*:?\(NSRegularExpression.escapedPattern(for: tag))>",
            options: [.dotMatchesLineSeparators]
        ) else { return nil }
        guard let m = pattern.firstMatch(in: xml, range: NSRange(xml.startIndex..., in: xml)),
              let r = Range(m.range(at: 1), in: xml)
        else { return nil }
        let value = String(xml[r]).trimmingCharacters(in: .whitespacesAndNewlines)
        return value.isEmpty ? nil : value
    }

    private func normalizeColor(_ raw: String?) -> String? {
        guard let raw else { return nil }
        let hex = raw.trimmingCharacters(in: .whitespaces).trimmingCharacters(in: CharacterSet(charactersIn: "#"))
        guard hex.count >= 6 else { return nil }
        return "#\(String(hex.prefix(6)).uppercased())"
    }

    // ─── REPORT request bodies ────────────────────────────────────────────────

    private func calendarQueryReport(
        from: Date? = nil,
        to: Date? = nil,
        componentType: String = "VEVENT"
    ) -> String {
        var timeFilter = ""
        if let from, let to {
            let fmt = ICalParser.formatDate
            timeFilter = #"<c:time-range start="\#(fmt(from))" end="\#(fmt(to))"/>"#
        }
        return """
            <?xml version="1.0" encoding="utf-8"?>
            <c:calendar-query xmlns:d="DAV:" xmlns:c="urn:ietf:params:xml:ns:caldav">
              <d:prop>
                <d:getetag/>
                <c:calendar-data/>
              </d:prop>
              <c:filter>
                <c:comp-filter name="VCALENDAR">
                  <c:comp-filter name="\(componentType)">
                    \(timeFilter)
                  </c:comp-filter>
                </c:comp-filter>
              </c:filter>
            </c:calendar-query>
            """
    }

    private func todoQueryReport() -> String {
        """
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
        """
    }
}

// ─── Errors ───────────────────────────────────────────────────────────────────

enum CalDavError: LocalizedError {
    case noCredentials
    case invalidUrl(String)
    case httpError(Int)
    case missingHref(String)

    var errorDescription: String? {
        switch self {
        case .noCredentials:      return "No CalDAV credentials configured."
        case .invalidUrl(let u):  return "Invalid URL: \(u)"
        case .httpError(let c):   return "HTTP \(c)"
        case .missingHref(let u): return "No resource href for \(u)"
        }
    }
}
