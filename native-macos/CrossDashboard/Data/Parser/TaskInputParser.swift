import Foundation
import CrossDashboardKit

/// Intelligent single-line task input parser (Todoist-style).
///
/// Syntax:
///   !! meet friends #social tonight    → priority=medium, tag=social, due=today 21:00
///   !!! deploy hotfix tomorrow morning → priority=high, due=tomorrow 08:00
///   buy milk #errands                  → no priority, tag=errands, no due
///   call mom monday                    → due=next Monday 10:00
///
/// Priority markers:
///   !   → low (priority=9)
///   !!  → medium (priority=5)
///   !!! → high (priority=1)
///
/// Direct Swift port of TaskInputParser.kt.
enum TaskInputParser {

    static func parse(
        input: String,
        defaults: TaskDefaults = TaskDefaults(),
        now: Date = Date()
    ) -> ParsedTask {
        var text = input.trimmingCharacters(in: .whitespaces)

        // ─── Priority ─────────────────────────────────────────────────────
        let priority: Int
        if text.hasPrefix("!!!") {
            text = String(text.dropFirst(3)).trimmingCharacters(in: .whitespaces)
            priority = 1
        } else if text.hasPrefix("!!") {
            text = String(text.dropFirst(2)).trimmingCharacters(in: .whitespaces)
            priority = 5
        } else if text.hasPrefix("!") {
            text = String(text.dropFirst(1)).trimmingCharacters(in: .whitespaces)
            priority = 9
        } else {
            priority = 0
        }

        // ─── Tags ─────────────────────────────────────────────────────────
        let tagRegex = /\#(\w+)/
        let tags = text.matches(of: tagRegex).map { String($0.output.1).lowercased() }
        text = text.replacing(tagRegex, with: "")
        text = collapseSpaces(text)

        // ─── Due date ─────────────────────────────────────────────────────
        let lower = text.lowercased()
        let calendar = Calendar.current
        var dueDate: Date? = nil

        let today = calendar.startOfDay(for: now)

        if let due = extractDue(lower: lower, today: today, defaults: defaults, calendar: calendar) {
            dueDate = due
            text = stripTimeKeywords(text)
            text = collapseSpaces(text)
        }

        return ParsedTask(
            summary: text,
            priority: priority,
            categories: tags,
            due: dueDate
        )
    }

    // ─── Due extraction ───────────────────────────────────────────────────────

    private static func extractDue(
        lower: String,
        today: Date,
        defaults: TaskDefaults,
        calendar: Calendar
    ) -> Date? {
        func dayAt(_ date: Date, hour: Int) -> Date {
            calendar.date(bySettingHour: hour, minute: 0, second: 0, of: date)!
        }
        func tomorrow() -> Date {
            calendar.date(byAdding: .day, value: 1, to: today)!
        }

        if lower.contains("tomorrow morning") {
            return dayAt(tomorrow(), hour: defaults.morningHour)
        }
        if lower.contains("tomorrow afternoon") {
            return dayAt(tomorrow(), hour: defaults.afternoonHour)
        }
        if lower.contains("tomorrow night") {
            return dayAt(tomorrow(), hour: defaults.nightHour)
        }
        if lower.contains("tomorrow") {
            return dayAt(tomorrow(), hour: defaults.defaultHour)
        }
        if lower.contains("tonight") {
            return dayAt(today, hour: defaults.nightHour)
        }
        if lower.contains("today") {
            return dayAt(today, hour: defaults.defaultHour)
        }
        if lower.contains("next week") {
            let nextWeek = calendar.date(byAdding: .weekOfYear, value: 1, to: today)!
            return dayAt(nextWeek, hour: defaults.defaultHour)
        }

        // Weekday names — always next occurrence (never same day or past)
        let weekdays: [(String, Int)] = [
            ("monday", 2), ("tuesday", 3), ("wednesday", 4),
            ("thursday", 5), ("friday", 6), ("saturday", 7), ("sunday", 1),
        ]
        for (keyword, weekdayNum) in weekdays where lower.contains(keyword) {
            var components = DateComponents()
            components.weekday = weekdayNum
            guard var target = calendar.nextDate(
                after: today,
                matching: components,
                matchingPolicy: .nextTimePreservingSmallerComponents
            ) else { continue }
            if target == today {
                target = calendar.date(byAdding: .weekOfYear, value: 1, to: target)!
            }
            return dayAt(target, hour: defaults.defaultHour)
        }

        return nil
    }

    private static func stripTimeKeywords(_ text: String) -> String {
        let keywords = [
            "tomorrow morning", "tomorrow afternoon", "tomorrow night", "tomorrow",
            "tonight", "today", "next week",
            "monday", "tuesday", "wednesday", "thursday", "friday", "saturday", "sunday",
        ]
        var result = text
        for kw in keywords {
            // Word-boundary-aware replacement
            result = result.replacingOccurrences(
                of: "\\b\(NSRegularExpression.escapedPattern(for: kw))\\b",
                with: "",
                options: [.regularExpression, .caseInsensitive]
            )
        }
        return result
    }

    private static func collapseSpaces(_ text: String) -> String {
        text.components(separatedBy: .whitespaces)
            .filter { !$0.isEmpty }
            .joined(separator: " ")
            .trimmingCharacters(in: .whitespaces)
    }
}
