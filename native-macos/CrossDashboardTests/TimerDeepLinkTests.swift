import XCTest

@testable import CrossDashboardKit

final class TimerDeepLinkTests: XCTestCase {
    func testBareTimerURLPresentsPicker() throws {
        let request = try XCTUnwrap(TimerDeepLinkRequest(url: XCTUnwrap(URL(string: "crossdashboard://timer"))))

        XCTAssertEqual(request.action, .pick)
        XCTAssertEqual(request.targetType, .timer)
        XCTAssertNil(request.name)
    }

    func testNameStartsAnUnlinkedTimerByDefault() throws {
        let request = try XCTUnwrap(TimerDeepLinkRequest(
            url: XCTUnwrap(URL(string: "crossdashboard://timer?name=Write%20release%20notes&minutes=45"))
        ))

        XCTAssertEqual(request.action, .start)
        XCTAssertEqual(request.targetType, .timer)
        XCTAssertEqual(request.name, "Write release notes")
        XCTAssertEqual(request.minutes, 45)
    }

    func testExactTaskTargetIsPreserved() throws {
        let request = try XCTUnwrap(TimerDeepLinkRequest(
            url: XCTUnwrap(URL(string: "crossdashboard://timer?action=start&type=task&id=task-123"))
        ))

        XCTAssertEqual(request.action, .start)
        XCTAssertEqual(request.targetType, .task)
        XCTAssertEqual(request.targetID, "task-123")
    }

    func testControlActionsParseWithoutTargets() throws {
        for action in ["pause", "resume", "toggle", "stop", "skip"] {
            let request = try XCTUnwrap(TimerDeepLinkRequest(
                url: XCTUnwrap(URL(string: "crossdashboard://timer?action=\(action)"))
            ))
            XCTAssertEqual(request.action.rawValue, action)
        }
    }

    func testRejectsUnknownActionsAndInvalidDurations() {
        XCTAssertNil(TimerDeepLinkRequest(url: URL(string: "crossdashboard://timer?action=explode")!))
        XCTAssertNil(TimerDeepLinkRequest(url: URL(string: "crossdashboard://timer?type=event&name=Standup")!))
        XCTAssertNil(TimerDeepLinkRequest(url: URL(string: "crossdashboard://timer?name=Work&minutes=0")!))
        XCTAssertNil(TimerDeepLinkRequest(url: URL(string: "crossdashboard://timer?name=Work&minutes=1441")!))
    }
}
