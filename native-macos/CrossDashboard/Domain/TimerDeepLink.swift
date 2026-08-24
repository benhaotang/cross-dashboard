import Foundation

public enum TimerDeepLinkAction: String, Equatable, Sendable {
    case pick
    case start
    case pause
    case resume
    case toggle
    case stop
    case skip
}

public enum TimerDeepLinkTargetType: String, Equatable, Sendable {
    case timer
    case task
    case issue
}

public struct TimerDeepLinkRequest: Equatable, Sendable {
    public let action: TimerDeepLinkAction
    public let targetType: TimerDeepLinkTargetType
    public let targetID: String?
    public let name: String?
    public let minutes: Int?

    public init?(url: URL) {
        guard url.scheme?.lowercased() == "crossdashboard" else { return nil }
        guard ["timer", "pomodoro"].contains(url.host?.lowercased() ?? "") else { return nil }

        let components = URLComponents(url: url, resolvingAgainstBaseURL: false)
        let queryItems = components?.queryItems ?? []
        func value(_ name: String) -> String? {
            queryItems
                .first(where: { $0.name.caseInsensitiveCompare(name) == .orderedSame })?
                .value?
                .trimmingCharacters(in: .whitespacesAndNewlines)
                .nilIfEmpty
        }

        targetID = value("id")
        name = value("name")
        if let rawTargetType = value("type") {
            guard let parsedTargetType = TimerDeepLinkTargetType(rawValue: rawTargetType.lowercased()) else {
                return nil
            }
            targetType = parsedTargetType
        } else {
            targetType = .timer
        }

        if let rawMinutes = value("minutes") {
            guard let parsedMinutes = Int(rawMinutes), (1...(24 * 60)).contains(parsedMinutes) else {
                return nil
            }
            minutes = parsedMinutes
        } else {
            minutes = nil
        }

        if let rawAction = value("action") {
            guard let parsedAction = TimerDeepLinkAction(rawValue: rawAction.lowercased()) else {
                return nil
            }
            action = parsedAction
        } else if targetID != nil || name != nil {
            action = .start
        } else {
            action = .pick
        }
    }
}

private extension String {
    var nilIfEmpty: String? { isEmpty ? nil : self }
}
