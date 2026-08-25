import CrossDashboardKit
import Foundation
import Observation
import ServiceManagement

@Observable
@MainActor
final class BackgroundServiceController {
    static let shared = BackgroundServiceController()

    private let service = SMAppService.agent(plistName: BackgroundServiceContract.agentPlistName)

    private(set) var status: SMAppService.Status
    private(set) var lastError: String?

    private init() {
        status = service.status
    }

    var statusLabel: String {
        switch status {
        case .notRegistered:
            return "Not enabled"
        case .enabled:
            return "Enabled"
        case .requiresApproval:
            return "Approval required"
        case .notFound:
            return "Agent missing from app"
        @unknown default:
            return "Unknown"
        }
    }

    var isEnabled: Bool {
        status == .enabled
    }

    var requiresApproval: Bool {
        status == .requiresApproval
    }

    func refreshStatus() {
        status = service.status
    }

    func register() {
        lastError = nil
        do {
            try service.register()
        } catch {
            lastError = error.localizedDescription
        }
        refreshStatus()
        if isEnabled {
            reloadSchedule()
        }
    }

    func unregister() {
        lastError = nil
        do {
            try service.unregister()
        } catch {
            lastError = error.localizedDescription
        }
        refreshStatus()
    }

    func openLoginItemsSettings() {
        SMAppService.openSystemSettingsLoginItems()
    }

    nonisolated func ping() async -> Bool {
        await withCheckedContinuation { continuation in
            let result = PingResult(continuation: continuation)
            let connection = NSXPCConnection(
                machServiceName: BackgroundServiceContract.machServiceName,
                options: []
            )
            connection.remoteObjectInterface = NSXPCInterface(
                with: BackgroundAgentXPCProtocol.self
            )
            connection.interruptionHandler = {
                connection.invalidate()
                result.resume(returning: false)
            }
            connection.invalidationHandler = nil
            connection.resume()

            guard
                let proxy = connection.remoteObjectProxyWithErrorHandler({ _ in
                    connection.invalidate()
                    result.resume(returning: false)
                }) as? BackgroundAgentXPCProtocol
            else {
                connection.invalidate()
                result.resume(returning: false)
                return
            }
            proxy.ping { response in
                connection.invalidate()
                result.resume(returning: response == "crossdashboard-agent-ok")
            }
        }
    }

    nonisolated func syncNow() async -> BackgroundSyncReport? {
        await withCheckedContinuation { continuation in
            let result = SyncResult(continuation: continuation)
            let connection = makeConnection()
            connection.invalidationHandler = {
                result.resume(returning: nil)
            }
            connection.interruptionHandler = {
                connection.invalidate()
                result.resume(returning: nil)
            }
            connection.resume()

            guard let proxy = connection.remoteObjectProxyWithErrorHandler({ _ in
                connection.invalidate()
                result.resume(returning: nil)
            }) as? BackgroundAgentXPCProtocol else {
                connection.invalidate()
                result.resume(returning: nil)
                return
            }
            proxy.syncNow { data in
                connection.invalidate()
                result.resume(returning: try? JSONDecoder().decode(BackgroundSyncReport.self, from: data))
            }
        }
    }

    nonisolated func reloadSchedule() {
        let connection = makeConnection()
        connection.resume()
        guard let proxy = connection.remoteObjectProxyWithErrorHandler { _ in
            connection.invalidate()
        } as? BackgroundAgentXPCProtocol else {
            connection.invalidate()
            return
        }
        proxy.reloadSchedule {
            connection.invalidate()
        }
    }

    nonisolated func reloadPomodoro() {
        let connection = makeConnection()
        connection.resume()
        guard let proxy = connection.remoteObjectProxyWithErrorHandler { _ in
            connection.invalidate()
        } as? BackgroundAgentXPCProtocol else {
            connection.invalidate()
            return
        }
        proxy.reloadPomodoro {
            connection.invalidate()
        }
    }

    nonisolated private func makeConnection() -> NSXPCConnection {
        let connection = NSXPCConnection(
            machServiceName: BackgroundServiceContract.machServiceName,
            options: []
        )
        connection.remoteObjectInterface = NSXPCInterface(with: BackgroundAgentXPCProtocol.self)
        connection.invalidationHandler = nil
        return connection
    }
}

private final class PingResult: @unchecked Sendable {
    private let lock = NSLock()
    private var continuation: CheckedContinuation<Bool, Never>?

    init(continuation: CheckedContinuation<Bool, Never>) {
        self.continuation = continuation
    }

    func resume(returning value: Bool) {
        lock.lock()
        let pending = continuation
        continuation = nil
        lock.unlock()
        pending?.resume(returning: value)
    }
}

private final class SyncResult: @unchecked Sendable {
    private let lock = NSLock()
    private var continuation: CheckedContinuation<BackgroundSyncReport?, Never>?

    init(continuation: CheckedContinuation<BackgroundSyncReport?, Never>) {
        self.continuation = continuation
    }

    func resume(returning value: BackgroundSyncReport?) {
        lock.lock()
        let pending = continuation
        continuation = nil
        lock.unlock()
        pending?.resume(returning: value)
    }
}
