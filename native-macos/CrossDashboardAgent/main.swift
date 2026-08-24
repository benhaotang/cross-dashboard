import CrossDashboardKit
import Foundation

private final class BackgroundAgent: NSObject, BackgroundAgentXPCProtocol, NSXPCListenerDelegate {
    func ping(withReply reply: @escaping (String) -> Void) {
        reply("crossdashboard-agent-ok")
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

private let delegate = BackgroundAgent()
let listener = NSXPCListener(machServiceName: BackgroundServiceContract.machServiceName)
listener.delegate = delegate
listener.resume()
RunLoop.main.run()
