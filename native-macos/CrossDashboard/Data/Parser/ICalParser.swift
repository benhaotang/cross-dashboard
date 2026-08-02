import Foundation
import CrossDashboardKit

/// Hand-written iCalendar (RFC 5545) parser for VEVENT, VTODO, and VJOURNAL components.
///
/// Handles:
/// - Line unfolding (CRLF + whitespace continuation)
/// - DTSTART / DTEND / DUE with TZID param and UTC Z suffix
/// - All-day dates (VALUE=DATE, no time component)
/// - RELATED-TO;RELTYPE=PARENT for subtask hierarchy
/// - CATEGORIES comma-separated list
///
/// Direct Swift port of ICalParser.kt.
enum ICalParser {

    // ─── Public entry points ─────────────────────────────────────────────────

    static func parseEvents(
        icalText: String,
        calendarHref: String? = nil,
        resourceHref: String? = nil,
        resourceEtag: String? = nil
    ) -> [CalendarEvent] {
        var results: [CalendarEvent] = []
        forEachComponent(in: icalText, named: "VEVENT") { props in
            if let event = parseEvent(props, calendarHref: calendarHref, resourceHref: resourceHref, resourceEtag: resourceEtag) {
                results.append(event)
            }
        }
        return results
    }

    static func parseTasks(
        icalText: String,
        calendarHref: String? = nil,
        resourceHref: String? = nil,
        resourceEtag: String? = nil
    ) -> [CalDavTask] {
        var results: [CalDavTask] = []
        forEachComponent(in: icalText, named: "VTODO") { props in
            if let task = parseTask(props, calendarHref: calendarHref, resourceHref: resourceHref, resourceEtag: resourceEtag) {
                results.append(task)
            }
        }
        return results
    }

    static func parseNotes(
        icalText: String,
        calendarHref: String? = nil,
        resourceHref: String? = nil,
        resourceEtag: String? = nil
    ) -> [Note] {
        var results: [Note] = []
        forEachComponent(in: icalText, named: "VJOURNAL") { props in
            if let note = parseNote(props, calendarHref: calendarHref, resourceHref: resourceHref, resourceEtag: resourceEtag) {
                results.append(note)
            }
        }
        return results
    }

    // ─── Component iterator ──────────────────────────────────────────────────

    private static func forEachComponent(
        in icalText: String,
        named componentName: String,
        block: ([String: String]) -> Void
    ) {
        let lines = unfold(icalText)
        var inside = false
        var props: [String: String] = [:]
        let beginMarker = "BEGIN:\(componentName)".uppercased()
        let endMarker   = "END:\(componentName)".uppercased()

        for line in lines {
            let upper = line.uppercased()
            if upper == beginMarker {
                inside = true
                props = [:]
            } else if upper == endMarker && inside {
                inside = false
                block(props)
            } else if inside {
                if let colonIdx = line.firstIndex(of: ":") {
                    let rawName = String(line[line.startIndex ..< colonIdx])
                    let value   = String(line[line.index(after: colonIdx)...])
                    let name    = normalizePropertyName(rawName)

                    if name == "RELATED-TO" || name == "ATTENDEE" {
                        let existing = props[name]
                        props[name] = existing == nil ? value : "\(existing!),\(value)"
                    } else if props[name] == nil {
                        props[name] = value
                    }
                    // Store raw line for TZID extraction
                    props["__RAW_\(name)"] = line
                }
            }
        }
    }

    private static func normalizePropertyName(_ rawName: String) -> String {
        if let semiIdx = rawName.firstIndex(of: ";") {
            return String(rawName[rawName.startIndex ..< semiIdx]).uppercased()
        }
        return rawName.uppercased()
    }

    // ─── Event parser ─────────────────────────────────────────────────────────

    private static func parseEvent(
        _ props: [String: String],
        calendarHref: String?,
        resourceHref: String?,
        resourceEtag: String?
    ) -> CalendarEvent? {
        guard let uid     = props["UID"],
              let rawSum  = props["SUMMARY"]
        else { return nil }

        let summary = unescape(rawSum)
        guard let start = parseDate(props, key: "DTSTART") else { return nil }

        let end: Date
        if let e = parseDate(props, key: "DTEND") {
            end = e
        } else if let durStr = props["DURATION"] {
            end = start.addingTimeInterval(parseDurationSeconds(durStr))
        } else {
            end = start.addingTimeInterval(3600)
        }

        return CalendarEvent(
            uid: uid,
            summary: summary,
            start: start,
            end: end,
            description: props["DESCRIPTION"].map { unescape($0) },
            location: props["LOCATION"].map { unescape($0) },
            calendarHref: calendarHref,
            etag: resourceEtag,
            href: resourceHref
        )
    }

    // ─── Task parser ──────────────────────────────────────────────────────────

    private static func parseTask(
        _ props: [String: String],
        calendarHref: String?,
        resourceHref: String?,
        resourceEtag: String?
    ) -> CalDavTask? {
        guard let uid     = props["UID"],
              let rawSum  = props["SUMMARY"]
        else { return nil }

        let summary = unescape(rawSum)

        var parentUid: String? = nil
        if let relatedTo = props["RELATED-TO"] {
            let rawLine = props["__RAW_RELATED-TO"] ?? ""
            if rawLine.uppercased().contains("RELTYPE=PARENT") {
                parentUid = relatedTo.split(separator: ",").first.map(String.init)
            }
        }

        return CalDavTask(
            uid: uid,
            summary: summary,
            description: props["DESCRIPTION"].map { unescape($0) },
            status: TaskStatus.fromIcal(props["STATUS"] ?? "NEEDS-ACTION"),
            priority: Int(props["PRIORITY"] ?? "") ?? 0,
            percentComplete: Int(props["PERCENT-COMPLETE"] ?? "") ?? 0,
            due: parseDate(props, key: "DUE"),
            dtstart: parseDate(props, key: "DTSTART"),
            completed: parseDate(props, key: "COMPLETED"),
            created: parseDate(props, key: "CREATED") ?? Date(),
            lastModified: parseDate(props, key: "LAST-MODIFIED") ?? Date(),
            categories: parseCategories(props["CATEGORIES"]),
            location: props["LOCATION"].map { unescape($0) },
            parentUid: parentUid,
            calendarHref: calendarHref,
            etag: resourceEtag,
            href: resourceHref
        )
    }

    // ─── Note (VJOURNAL) parser ───────────────────────────────────────────────

    private static func parseNote(
        _ props: [String: String],
        calendarHref: String?,
        resourceHref: String?,
        resourceEtag: String?
    ) -> Note? {
        guard let uid    = props["UID"],
              let rawSum = props["SUMMARY"]
        else { return nil }

        return Note(
            uid: uid,
            summary: unescape(rawSum),
            body: props["DESCRIPTION"].map { unescape($0) } ?? "",
            categories: parseCategories(props["CATEGORIES"]),
            created: parseDate(props, key: "DTSTAMP") ?? Date(),
            lastModified: parseDate(props, key: "LAST-MODIFIED") ?? Date(),
            calendarHref: calendarHref,
            etag: resourceEtag,
            href: resourceHref
        )
    }

    // ─── iCal serializers (domain → iCal text) ───────────────────────────────

    static func serializeTask(_ task: CalDavTask) -> String {
        var lines: [String] = []
        func line(_ s: String) { lines.append(s) }
        line("BEGIN:VCALENDAR")
        line("VERSION:2.0")
        line("PRODID:-//CrossDashboard//Native//EN")
        line("BEGIN:VTODO")
        line("UID:\(task.uid)")
        line("DTSTAMP:\(formatDate(Date()))")
        line("CREATED:\(formatDate(task.created))")
        line("LAST-MODIFIED:\(formatDate(Date()))")
        line("SUMMARY:\(escape(task.summary))")
        line("STATUS:\(task.status.rawValue)")
        line("PRIORITY:\(task.priority)")
        line("PERCENT-COMPLETE:\(task.percentComplete)")
        if let desc = task.description { line("DESCRIPTION:\(escape(desc))") }
        if let due  = task.due         { line("DUE:\(formatDate(due))") }
        if let dts  = task.dtstart     { line("DTSTART:\(formatDate(dts))") }
        if let comp = task.completed   { line("COMPLETED:\(formatDate(comp))") }
        if !task.categories.isEmpty    { line("CATEGORIES:\(task.categories.joined(separator: ","))") }
        if let loc  = task.location    { line("LOCATION:\(escape(loc))") }
        if let par  = task.parentUid   { line("RELATED-TO;RELTYPE=PARENT:\(par)") }
        line("END:VTODO")
        line("END:VCALENDAR")
        return lines.joined(separator: "\r\n") + "\r\n"
    }

    static func serializeNote(_ note: Note) -> String {
        var lines: [String] = []
        func line(_ s: String) { lines.append(s) }
        line("BEGIN:VCALENDAR")
        line("VERSION:2.0")
        line("PRODID:-//CrossDashboard//Native//EN")
        line("BEGIN:VJOURNAL")
        line("UID:\(note.uid)")
        line("DTSTAMP:\(formatDate(Date()))")
        line("LAST-MODIFIED:\(formatDate(Date()))")
        line("SUMMARY:\(escape(note.summary))")
        if !note.body.isEmpty           { line("DESCRIPTION:\(escape(note.body))") }
        if !note.categories.isEmpty     { line("CATEGORIES:\(note.categories.joined(separator: ","))") }
        line("END:VJOURNAL")
        line("END:VCALENDAR")
        return lines.joined(separator: "\r\n") + "\r\n"
    }

    static func serializeEvent(_ event: CalendarEvent) -> String {
        var lines: [String] = []
        func line(_ s: String) { lines.append(s) }
        line("BEGIN:VCALENDAR")
        line("VERSION:2.0")
        line("PRODID:-//CrossDashboard//Native//EN")
        line("BEGIN:VEVENT")
        line("UID:\(event.uid)")
        line("DTSTAMP:\(formatDate(Date()))")
        line("DTSTART:\(formatDate(event.start))")
        line("DTEND:\(formatDate(event.end))")
        line("SUMMARY:\(escape(event.summary))")
        if let desc = event.description { line("DESCRIPTION:\(escape(desc))") }
        if let loc  = event.location    { line("LOCATION:\(escape(loc))") }
        line("LAST-MODIFIED:\(formatDate(Date()))")
        line("END:VEVENT")
        line("END:VCALENDAR")
        return lines.joined(separator: "\r\n") + "\r\n"
    }

    // ─── Parsing helpers ─────────────────────────────────────────────────────

    private static func unfold(_ icalText: String) -> [String] {
        var result: [String] = []
        var current = ""
        let newline  = CharacterSet.newlines
        for line in icalText.components(separatedBy: .newlines) {
            if line.hasPrefix(" ") || line.hasPrefix("\t") {
                current += line.dropFirst()
            } else {
                if !current.isEmpty { result.append(current) }
                current = line
            }
        }
        if !current.isEmpty { result.append(current) }
        return result
    }

    // Formats: 20231015T143000Z or 20231015
    private static let utcFormatter: DateFormatter = {
        let f = DateFormatter()
        f.dateFormat = "yyyyMMdd'T'HHmmss'Z'"
        f.timeZone = TimeZone(identifier: "UTC")
        f.locale = Locale(identifier: "en_US_POSIX")
        return f
    }()

    private static let localFormatter: DateFormatter = {
        let f = DateFormatter()
        f.dateFormat = "yyyyMMdd'T'HHmmss"
        f.locale = Locale(identifier: "en_US_POSIX")
        return f
    }()

    private static let dateOnlyFormatter: DateFormatter = {
        let f = DateFormatter()
        f.dateFormat = "yyyyMMdd"
        f.timeZone = TimeZone(identifier: "UTC")
        f.locale = Locale(identifier: "en_US_POSIX")
        return f
    }()

    private static func parseDate(_ props: [String: String], key: String) -> Date? {
        guard let rawLine = props["__RAW_\(key)"],
              let colonIdx = rawLine.firstIndex(of: ":") else { return nil }

        // Keep rawParamPart in original case so IANA timezone identifiers (e.g. "Europe/Berlin")
        // are preserved for TimeZone(identifier:), which is case-sensitive.
        let rawParamPart = String(rawLine[rawLine.startIndex ..< colonIdx])
        let paramPart    = rawParamPart.uppercased()   // uppercased only for keyword checks
        let value        = String(rawLine[rawLine.index(after: colonIdx)...]).trimmingCharacters(in: .whitespaces)

        // All-day: VALUE=DATE
        if paramPart.contains("VALUE=DATE") {
            dateOnlyFormatter.timeZone = .current
            return dateOnlyFormatter.date(from: String(value.prefix(8)))
        }

        // UTC Z suffix
        if value.hasSuffix("Z") {
            // Normalize to "yyyyMMdd'T'HHmmss'Z'" — handle compact form
            let compact = value.replacingOccurrences(of: "-", with: "")
                               .replacingOccurrences(of: ":", with: "")
            if compact.count >= 15 {
                let normalized = "\(compact.prefix(8))T\(compact.dropFirst(9).prefix(6))Z"
                return utcFormatter.date(from: normalized) ?? utcFormatter.date(from: value)
            }
            return utcFormatter.date(from: value)
        }

        // TZID param — search rawParamPart case-insensitively so the captured value retains
        // its original casing (e.g. "Europe/Berlin", not "EUROPE/BERLIN").
        // RFC 5545: a datetime with no Z and no TZID is "floating" and should be interpreted
        // in the local system timezone, not UTC.
        var tz: TimeZone = .current
        if paramPart.contains("TZID="),
           let tzRange = rawParamPart.range(of: "TZID=", options: .caseInsensitive) {
            let remaining = rawParamPart[tzRange.upperBound...]
            let tzId: String
            if let endIdx = remaining.firstIndex(where: { $0 == ";" || $0 == ":" }) {
                tzId = String(remaining[remaining.startIndex ..< endIdx])
            } else {
                tzId = String(remaining)
            }
            tz = TimeZone(identifier: tzId) ?? .current
        }

        localFormatter.timeZone = tz
        return localFormatter.date(from: value)
    }

    private static func parseDurationSeconds(_ s: String) -> TimeInterval {
        var total: TimeInterval = 0
        let pattern = /P(?:(\d+)W)?(?:(\d+)D)?(?:T(?:(\d+)H)?(?:(\d+)M)?(?:(\d+)S)?)?/
        if let match = s.firstMatch(of: pattern) {
            let weeks   = Double(match.output.1 ?? Substring("")) ?? 0
            let days    = Double(match.output.2 ?? Substring("")) ?? 0
            let hours   = Double(match.output.3 ?? Substring("")) ?? 0
            let minutes = Double(match.output.4 ?? Substring("")) ?? 0
            let seconds = Double(match.output.5 ?? Substring("")) ?? 0
            total = weeks * 7 * 86400 + days * 86400 + hours * 3600 + minutes * 60 + seconds
        }
        return total > 0 ? total : 3600
    }

    private static func parseCategories(_ raw: String?) -> [String] {
        guard let raw, !raw.isEmpty else { return [] }
        return raw.split(separator: ",").map { $0.trimmingCharacters(in: .whitespaces) }.filter { !$0.isEmpty }
    }

    static func unescape(_ value: String) -> String {
        value
            .replacingOccurrences(of: "\\n", with: "\n")
            .replacingOccurrences(of: "\\N", with: "\n")
            .replacingOccurrences(of: "\\,", with: ",")
            .replacingOccurrences(of: "\\;", with: ";")
            .replacingOccurrences(of: "\\\\", with: "\\")
    }

    private static func escape(_ value: String) -> String {
        value
            .replacingOccurrences(of: "\\", with: "\\\\")
            .replacingOccurrences(of: "\n", with: "\\n")
            .replacingOccurrences(of: ",",  with: "\\,")
            .replacingOccurrences(of: ";",  with: "\\;")
    }

    static func formatDate(_ date: Date) -> String {
        utcFormatter.string(from: date)
    }
}
