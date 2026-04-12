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

    private init() {}

    // ─── Public API ───────────────────────────────────────────────────────────

    func start(for task: CalDavTask? = nil) {
        let settings = container.preferences.pomodoroSettings
        currentTask = task
        state = PomodoroState(
            phase: .work,
            secondsLeft: settings.workMinutes * 60,
            running: true,
            currentSession: 1,
            completedSessions: 0,
            itemTitle: task?.summary ?? "",
            active: true,
            settings: settings
        )
        scheduleTimer()
        PomodoroStatusItem.shared.show()
    }

    func startForIssue(title: String) {
        let settings = container.preferences.pomodoroSettings
        currentTask = nil
        state = PomodoroState(
            phase: .work,
            secondsLeft: settings.workMinutes * 60,
            running: true,
            currentSession: 1,
            completedSessions: 0,
            itemTitle: title,
            active: true,
            settings: settings
        )
        scheduleTimer()
        PomodoroStatusItem.shared.show()
    }

    func pause() {
        guard state.running else { return }
        timer?.invalidate()
        timer = nil
        state.running = false
    }

    func resume() {
        guard !state.running, state.active else { return }
        state.running = true
        scheduleTimer()
    }

    func stop() {
        timer?.invalidate()
        timer = nil
        state = PomodoroState()
        currentTask = nil
        PomodoroStatusItem.shared.hide()
    }

    func skip() {
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
                await container.statsRepository.incrementPomodoro()
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
