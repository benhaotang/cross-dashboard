import CrossDashboardKit
import Foundation
import SwiftData
import UserNotifications

@MainActor
final class AgentPomodoroCoordinator {
    static let shared = AgentPomodoroCoordinator()

    private let context = PersistenceController.shared.container.mainContext
    private let statsRepository: StatsRepository
    private let taskRepository: TaskRepository
    private var timer: Timer?

    private init() {
        let stats = StatsRepository(context: context)
        statsRepository = stats
        taskRepository = TaskRepository(
            context: context,
            client: CalDavClient(keychain: .shared),
            statsRepo: stats
        )
        reload()
    }

    func reload() {
        timer?.invalidate()
        timer = nil
        guard PomodoroSessionStore.load() != nil else { return }
        timer = Timer.scheduledTimer(withTimeInterval: 1, repeats: true) { _ in
            Task { @MainActor in
                await AgentPomodoroCoordinator.shared.tick()
            }
        }
        if let timer { RunLoop.main.add(timer, forMode: .common) }
        Task { await tick() }
    }

    private func tick() async {
        guard let session = PomodoroSessionStore.load() else {
            timer?.invalidate()
            timer = nil
            return
        }
        guard !session.isPaused, session.secondsRemaining(at: Date()) == 0 else { return }
        if session.phase == .work {
            statsRepository.incrementPomodoro()
            await logCompletedWorkSession(session)
        }
        let next = advanced(session)
        try? PomodoroSessionStore.save(next)
        await notifyPhaseChange(next.phase)
        postChange()
    }

    private func advanced(_ session: PersistedPomodoroSession) -> PersistedPomodoroSession {
        var next = session
        let now = Date()
        let duration: Int
        switch session.phase {
        case .work:
            next.completedSessions += 1
            if next.completedSessions % max(1, next.settings.sessionsUntilLongBreak) == 0 {
                next.phase = .longBreak
                duration = next.settings.longBreakMinutes * 60
            } else {
                next.phase = .shortBreak
                duration = next.settings.shortBreakMinutes * 60
            }
        case .shortBreak, .longBreak:
            next.phase = .work
            duration = next.settings.workMinutes * 60
        }
        next.phaseStartedAt = now
        next.deadline = now.addingTimeInterval(TimeInterval(duration))
        next.isPaused = false
        next.pausedSecondsRemaining = nil
        return next
    }

    private func logCompletedWorkSession(_ session: PersistedPomodoroSession) async {
        guard session.targetKind == .task else { return }
        taskRepository.loadFromDB()
        guard let task = taskRepository.allTasks.first(where: { $0.uid == session.targetID }) else { return }
        let line = "Pomodoro session completed at \(Date().formatted(date: .abbreviated, time: .shortened))"
        let updated = CalDavTask(
            uid: task.uid,
            summary: task.summary,
            description: [task.description, line].compactMap { $0 }.joined(separator: "\n"),
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
        try? await taskRepository.update(updated)
    }

    private func notifyPhaseChange(_ phase: PomodoroPhase) async {
        let content = UNMutableNotificationContent()
        content.title = phase == .work ? "Focus" : "Break"
        content.sound = .default
        let request = UNNotificationRequest(
            identifier: "pomodoro-phase",
            content: content,
            trigger: nil
        )
        try? await UNUserNotificationCenter.current().add(request)
    }

    private func postChange() {
        DistributedNotificationCenter.default().postNotificationName(
            Notification.Name(BackgroundServiceContract.pomodoroDidChangeNotification),
            object: nil,
            userInfo: nil,
            deliverImmediately: true
        )
    }
}
