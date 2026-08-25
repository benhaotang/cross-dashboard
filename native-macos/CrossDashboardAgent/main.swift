import CrossDashboardKit
import Foundation

private final class BackgroundAgent: NSObject, BackgroundAgentXPCProtocol, NSXPCListenerDelegate {
    func ping(withReply reply: @escaping (String) -> Void) {
        reply("crossdashboard-agent-ok")
    }

    func syncNow(withReply reply: @escaping (Data) -> Void) {
        let reply = DataReply(reply)
        Task { @MainActor in
            let report = await AgentSyncCoordinator.shared.syncNow()
            reply.call((try? JSONEncoder().encode(report)) ?? Data())
        }
    }

    func reloadSchedule(withReply reply: @escaping () -> Void) {
        let reply = VoidReply(reply)
        Task { @MainActor in
            AgentSyncCoordinator.shared.reloadSchedule()
            reply.call()
        }
    }

    func reloadPomodoro(withReply reply: @escaping () -> Void) {
        let reply = VoidReply(reply)
        Task { @MainActor in
            AgentPomodoroCoordinator.shared.reload()
            reply.call()
        }
    }

    func listener(
        _ listener: NSXPCListener,
        shouldAcceptNewConnection newConnection: NSXPCConnection
    ) -> Bool {
        newConnection.exportedInterface = NSXPCInterface(with: BackgroundAgentXPCProtocol.self)
        newConnection.exportedObject = self
        newConnection.resume()
        return true
    }
}

private final class DataReply: @unchecked Sendable {
    private let reply: (Data) -> Void

    init(_ reply: @escaping (Data) -> Void) {
        self.reply = reply
    }

    func call(_ data: Data) {
        reply(data)
    }
}

private final class VoidReply: @unchecked Sendable {
    private let reply: () -> Void

    init(_ reply: @escaping () -> Void) {
        self.reply = reply
    }

    func call() {
        reply()
    }
}

private let delegate = BackgroundAgent()
let listener = NSXPCListener(machServiceName: BackgroundServiceContract.machServiceName)
listener.delegate = delegate
listener.resume()
Task { @MainActor in
    _ = AgentSyncCoordinator.shared
    _ = AgentPomodoroCoordinator.shared
}
RunLoop.main.run()
