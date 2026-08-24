import Foundation

public enum BackgroundServiceContract {
    public static let agentPlistName = "group.com.crossdashboard.background-agent.plist"
    public static let machServiceName = "group.com.crossdashboard.background-agent"
}

@objc public protocol BackgroundAgentXPCProtocol {
    func ping(withReply reply: @escaping (String) -> Void)
}
