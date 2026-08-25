import Foundation
import Observation
import AppKit
import CrossDashboardKit

/// Singleton timer state for the Pomodoro technique.
/// Mirrors `PomodoroViewModel.kt` (Hilt @Singleton) on Android.
///
/// Owned by `CrossDashboardApp` and injected via `@Environment(PomodoroViewModel.self)`.
/// Also accessible directly via `PomodoroViewModel.shared` for AppKit code (NSStatusItem).
@Observable
@MainActor
final class PomodoroViewModel {

    // ─── Singleton ────────────────────────────────────────────────────────────

    static let shared = PomodoroViewModel()

    // ─── State ────────────────────────────────────────────────────────────────

    private(set) var state: PomodoroState = PomodoroState()

    /// Task being tracked (nil when no Pomodoro is active)
    private(set) var currentTask: CalDavTask?

    // ─── Private ──────────────────────────────────────────────────────────────

    private var timer: Timer?
    private let container = AppContainer.shared
    private var serviceObserver: NSObjectProtocol?

    private init() {
        serviceObserver = DistributedNotificationCenter.default().addObserver(
            forName: Notification.Name(BackgroundServiceContract.pomodoroDidChangeNotification),
            object: nil,
            queue: .main
        ) { [weak self] _ in
            Task { @MainActor [weak self] in self?.refreshFromServiceStore() }
        }
        if let session = PomodoroSessionStore.load() {
            apply(session)
            scheduleServiceDisplayTimer()
        }
    }

    // ─── Public API ───────────────────────────────────────────────────────────

    @discardableResult
    func start(for task: CalDavTask? = nil, minutes: Int? = nil) -> Bool {
        start(title: task?.summary ?? "", task: task, minutes: minutes)
    }

    @discardableResult
    func startNamed(_ name: String, minutes: Int? = nil) -> Bool {
        start(title: name, task: nil, minutes: minutes)
    }

    @discardableResult
    func startForIssue(title: String, id: String = "", minutes: Int? = nil) -> Bool {
        start(title: title, task: nil, issueID: id, minutes: minutes)
    }

    private func start(
        title: String,
        task: CalDavTask?,
        issueID: String = "",
        minutes: Int?
    ) -> Bool {
        guard !state.active else { return false }
        let settings = container.preferences.pomodoroSettings
        let focusMinutes = min(max(minutes ?? settings.workMinutes, 1), 24 * 60)
        currentTask = task
        if backgroundServiceEnabled {
            let now = Date()
            let session = PersistedPomodoroSession(
                phase: .work,
                targetKind: task != nil ? .task : (issueID.isEmpty ? .none : .issue),
                targetID: task?.uid ?? issueID,
                title: title,
                phaseStartedAt: now,
                deadline: now.addingTimeInterval(TimeInterval(focusMinutes * 60)),
                isPaused: false,
                settings: settings
            )
            do {
                try PomodoroSessionStore.save(session)
            } catch {
                return false
            }
            apply(session)
            scheduleServiceDisplayTimer()
            BackgroundServiceController.shared.reloadPomodoro()
            PomodoroStatusItem.shared.show()
            return true
        }
        state = PomodoroState(
            phase: .work,
            secondsLeft: focusMinutes * 60,
            running: true,
            currentSession: 1,
            completedSessions: 0,
            itemTitle: title,
            active: true,
            settings: settings
        )
        scheduleTimer()
        PomodoroStatusItem.shared.show()
        return true
    }

    func pause() {
        guard state.running else { return }
        if backgroundServiceEnabled, var session = PomodoroSessionStore.load() {
            session.pausedSecondsRemaining = session.secondsRemaining(at: Date())
            session.deadline = nil
            session.isPaused = true
            try? PomodoroSessionStore.save(session)
            apply(session)
            BackgroundServiceController.shared.reloadPomodoro()
            return
        }
        timer?.invalidate()
        timer = nil
        state.running = false
    }

    func resume() {
        guard !state.running, state.active else { return }
        if backgroundServiceEnabled, var session = PomodoroSessionStore.load() {
            let remaining = max(1, session.pausedSecondsRemaining ?? 0)
            session.phaseStartedAt = Date()
            session.deadline = Date().addingTimeInterval(TimeInterval(remaining))
            session.isPaused = false
            session.pausedSecondsRemaining = nil
            try? PomodoroSessionStore.save(session)
            apply(session)
            scheduleServiceDisplayTimer()
            BackgroundServiceController.shared.reloadPomodoro()
            return
        }
        state.running = true
        scheduleTimer()
    }

    func stop() {
        if backgroundServiceEnabled {
            PomodoroSessionStore.clear()
            BackgroundServiceController.shared.reloadPomodoro()
        }
        timer?.invalidate()
        timer = nil
        state = PomodoroState()
        currentTask = nil
        PomodoroStatusItem.shared.hide()
    }

    func skip() {
        if backgroundServiceEnabled, let session = PomodoroSessionStore.load() {
            let next = skipped(session)
            try? PomodoroSessionStore.save(next)
            apply(next)
            scheduleServiceDisplayTimer()
            BackgroundServiceController.shared.reloadPomodoro()
            return
        }
        advancePhase()
    }

    // ─── Timer tick ───────────────────────────────────────────────────────────

    private func scheduleTimer() {
        timer?.invalidate()
        timer = Timer.scheduledTimer(withTimeInterval: 1, repeats: true) { [weak self] _ in
            Task { @MainActor [weak self] in
                self?.tick()
            }
        }
        RunLoop.main.add(timer!, forMode: .common)
    }

    private func scheduleServiceDisplayTimer() {
        timer?.invalidate()
        timer = Timer.scheduledTimer(withTimeInterval: 1, repeats: true) { [weak self] _ in
            Task { @MainActor [weak self] in self?.refreshFromServiceStore() }
        }
        if let timer { RunLoop.main.add(timer, forMode: .common) }
    }

    private func refreshFromServiceStore() {
        guard let session = PomodoroSessionStore.load() else {
            timer?.invalidate()
            timer = nil
            state = PomodoroState()
            currentTask = nil
            PomodoroStatusItem.shared.hide()
            return
        }
        apply(session)
        if state.active { PomodoroStatusItem.shared.show() }
    }

    private func apply(_ session: PersistedPomodoroSession) {
        if session.targetKind == .task {
            currentTask = container.taskRepository.allTasks.first { $0.uid == session.targetID }
        } else {
            currentTask = nil
        }
        state = PomodoroState(
            phase: session.phase,
            secondsLeft: session.secondsRemaining(at: Date()),
            running: !session.isPaused,
            currentSession: session.completedSessions + 1,
            completedSessions: session.completedSessions,
            itemTitle: session.title,
            active: true,
            settings: session.settings
        )
    }

    private func skipped(_ session: PersistedPomodoroSession) -> PersistedPomodoroSession {
        var next = session
        let duration: Int
        switch session.phase {
        case .work:
            if session.completedSessions > 0 &&
                session.completedSessions % max(1, session.settings.sessionsUntilLongBreak) == 0 {
                next.phase = .longBreak
                duration = session.settings.longBreakMinutes * 60
            } else {
                next.phase = .shortBreak
                duration = session.settings.shortBreakMinutes * 60
            }
        case .shortBreak, .longBreak:
            next.phase = .work
            duration = session.settings.workMinutes * 60
        }
        next.phaseStartedAt = Date()
        next.deadline = Date().addingTimeInterval(TimeInterval(duration))
        next.isPaused = false
        next.pausedSecondsRemaining = nil
        return next
    }

    private var backgroundServiceEnabled: Bool {
        let service = BackgroundServiceController.shared
        service.refreshStatus()
        return service.isEnabled
    }

    private func tick() {
        guard state.running else { return }
        if state.secondsLeft > 0 {
            state.secondsLeft -= 1
        } else {
            phaseCompleted()
        }
    }

    private func phaseCompleted() {
        if state.phase == .work {
            // Log completed session
            Task {
                container.statsRepository.incrementPomodoro()
                if let task = currentTask {
                    logSessionToTask(task)
                }
            }
            state.completedSessions += 1
        }
        advancePhase()
    }

    private func advancePhase() {
        let settings = state.settings
        switch state.phase {
        case .work:
            if state.completedSessions > 0 && state.completedSessions % settings.sessionsUntilLongBreak == 0 {
                state.phase = .longBreak
                state.secondsLeft = settings.longBreakMinutes * 60
            } else {
                state.phase = .shortBreak
                state.secondsLeft = settings.shortBreakMinutes * 60
            }
        case .shortBreak, .longBreak:
            state.phase = .work
            state.secondsLeft = settings.workMinutes * 60
            state.currentSession += 1
        }
    }

    private func logSessionToTask(_ task: CalDavTask) {
        let formatter = DateFormatter()
        formatter.dateStyle = .short
        formatter.timeStyle = .short
        let timestamp = formatter.string(from: Date())
        let logLine = "🍅 Pomodoro session completed at \(timestamp)"
        let newDesc = [task.description, logLine]
            .compactMap { $0 }
            .joined(separator: "\n")
        let updated = CalDavTask(
            uid: task.uid,
            summary: task.summary,
            description: newDesc,
            status: task.status,
            priority: task.priority,
            percentComplete: task.percentComplete,
            due: task.due,
            dtstart: task.dtstart,
            completed: task.completed,
            created: task.created,
            lastModified: Date(),
            categories: task.categories,
            location: task.location,
            parentUid: task.parentUid,
            calendarHref: task.calendarHref,
            etag: task.etag,
            href: task.href
        )
        Task {
            _ = try? await container.taskRepository.update(updated)
        }
    }
}

// ─── Computed helpers ─────────────────────────────────────────────────────────

extension PomodoroViewModel {

    var timerLabel: String {
        let m = state.secondsLeft / 60
        let s = state.secondsLeft % 60
        return String(format: "%02d:%02d", m, s)
    }

    var menuBarTitle: String {
        "\(state.phase.label) — \(timerLabel)"
    }

    var phaseColor: PomodoroPhaseColor {
        switch state.phase {
        case .work:       return .red
        case .shortBreak: return .green
        case .longBreak:  return .blue
        }
    }
}

enum PomodoroPhaseColor {
    case red, green, blue

    var color: NSColor {
        switch self {
        case .red:   return .systemRed
        case .green: return .systemGreen
        case .blue:  return .systemBlue
        }
    }
}
