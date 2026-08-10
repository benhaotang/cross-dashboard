import Foundation

/// Process-local timezone selection. Automatic retains the OS timezone captured at launch.
@MainActor
public enum AppTimeZone {
    public static let system: TimeZone = {
        let identifier = TimeZone.autoupdatingCurrent.identifier
        return TimeZone(identifier: identifier) ?? TimeZone(secondsFromGMT: 0)!
    }()

    public static func applyOverride(_ identifier: String?) {
        let normalized = identifier?.trimmingCharacters(in: .whitespacesAndNewlines)
        let selected = normalized.flatMap { $0.isEmpty ? nil : TimeZone(identifier: $0) }
        NSTimeZone.default = selected ?? system
    }
}
