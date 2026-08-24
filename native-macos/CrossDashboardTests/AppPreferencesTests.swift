import XCTest

@testable import CrossDashboardKit

final class AppPreferencesTests: XCTestCase {

    @MainActor
    func testMigratesLegacyPreferencesAndBackgroundSettings() {
        let (legacy, legacyName) = makeDefaults("legacy")
        let (shared, sharedName) = makeDefaults("shared")
        defer {
            legacy.removePersistentDomain(forName: legacyName)
            shared.removePersistentDomain(forName: sharedName)
        }

        legacy.set(45, forKey: "pref_sync_interval")
        legacy.set("/tmp/background.heic", forKey: "desktop_background_image_path")

        _ = AppPreferences(defaults: shared, legacyDefaults: legacy)

        XCTAssertEqual(shared.integer(forKey: "pref_sync_interval"), 45)
        XCTAssertEqual(
            shared.string(forKey: "desktop_background_image_path"),
            "/tmp/background.heic"
        )
    }

    @MainActor
    func testMigrationPreservesValuesAlreadyWrittenToTheAppGroup() {
        let (legacy, legacyName) = makeDefaults("legacy")
        let (shared, sharedName) = makeDefaults("shared")
        defer {
            legacy.removePersistentDomain(forName: legacyName)
            shared.removePersistentDomain(forName: sharedName)
        }

        legacy.set(30, forKey: "pref_sync_interval")
        shared.set(90, forKey: "pref_sync_interval")

        let preferences = AppPreferences(defaults: shared, legacyDefaults: legacy)

        XCTAssertEqual(preferences.syncIntervalMinutes, 90)
        XCTAssertEqual(shared.integer(forKey: "pref_sync_interval"), 90)
    }

    private func makeDefaults(_ role: String) -> (UserDefaults, String) {
        let name = "com.crossdashboard.tests.\(role).\(UUID().uuidString)"
        let defaults = UserDefaults(suiteName: name)!
        defaults.removePersistentDomain(forName: name)
        return (defaults, name)
    }
}
