import XCTest

@testable import CrossDashboardKit

final class PersistedPomodoroSessionTests: XCTestCase {

    func testRunningSessionUsesAbsoluteDeadline() {
        let now = Date(timeIntervalSince1970: 1_000)
        let session = makeSession(
            deadline: now.addingTimeInterval(90.2),
            isPaused: false
        )

        XCTAssertEqual(session.secondsRemaining(at: now), 91)
        XCTAssertEqual(session.secondsRemaining(at: now.addingTimeInterval(200)), 0)
    }

    func testPausedSessionUsesStoredRemainingSeconds() {
        let now = Date(timeIntervalSince1970: 1_000)
        let session = makeSession(
            deadline: now.addingTimeInterval(10),
            isPaused: true,
            pausedSecondsRemaining: 75
        )

        XCTAssertEqual(session.secondsRemaining(at: now.addingTimeInterval(500)), 75)
    }

    func testRoundTripsThroughSharedDefaultsStore() throws {
        let suiteName = "com.crossdashboard.tests.pomodoro.\(UUID().uuidString)"
        let defaults = UserDefaults(suiteName: suiteName)!
        defer { defaults.removePersistentDomain(forName: suiteName) }
        let session = makeSession(deadline: Date(timeIntervalSince1970: 2_000), isPaused: false)

        try PomodoroSessionStore.save(session, defaults: defaults)

        XCTAssertEqual(PomodoroSessionStore.load(defaults: defaults), session)
        PomodoroSessionStore.clear(defaults: defaults)
        XCTAssertNil(PomodoroSessionStore.load(defaults: defaults))
    }

    private func makeSession(
        deadline: Date?,
        isPaused: Bool,
        pausedSecondsRemaining: Int? = nil
    ) -> PersistedPomodoroSession {
        PersistedPomodoroSession(
            phase: .work,
            phaseStartedAt: Date(timeIntervalSince1970: 900),
            deadline: deadline,
            isPaused: isPaused,
            pausedSecondsRemaining: pausedSecondsRemaining,
            settings: PomodoroSettings()
        )
    }
}
