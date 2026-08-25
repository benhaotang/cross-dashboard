import Foundation

public enum BackgroundServiceContract {
    public static let agentPlistName = "group.com.crossdashboard.background-agent.plist"
    public static let machServiceName = "group.com.crossdashboard.background-agent"
    public static let syncDidCompleteNotification = "com.crossdashboard.backgroundSyncDidComplete"
    public static let pomodoroDidChangeNotification = "com.crossdashboard.pomodoroDidChange"
}

@objc public protocol BackgroundAgentXPCProtocol {
    func ping(withReply reply: @escaping (String) -> Void)
    func syncNow(withReply reply: @escaping (Data) -> Void)
    func reloadSchedule(withReply reply: @escaping () -> Void)
    func reloadPomodoro(withReply reply: @escaping () -> Void)
}

public struct BackgroundSyncReport: Codable, Sendable {
    public let succeeded: Bool
    public let completedAt: Date
    public let failedSources: [String]

    public init(succeeded: Bool, completedAt: Date = Date(), failedSources: [String] = []) {
        self.succeeded = succeeded
        self.completedAt = completedAt
        self.failedSources = failedSources
    }
}
