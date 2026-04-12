import Foundation
import Security

/// Secure credential storage backed by macOS Keychain Services.
///
/// All values are stored as `kSecClassGenericPassword` items under the service name
/// "com.crossdashboard.app". Mirrors the API of `SecureStore.kt` on Android.
final class KeychainStore: Sendable {
    static let shared = KeychainStore()

    private let service = "com.crossdashboard.app"

    private init() {}

    func set(_ key: String, value: String) {
        guard let data = value.data(using: .utf8) else { return }
        let query: [CFString: Any] = [
            kSecClass:       kSecClassGenericPassword,
            kSecAttrService: service,
            kSecAttrAccount: key,
        ]
        let attributes: [CFString: Any] = [
            kSecValueData: data,
            kSecAttrAccessible: kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly,
        ]
        let status = SecItemCopyMatching(query as CFDictionary, nil)
        if status == errSecItemNotFound {
            var addQuery = query
            addQuery[kSecValueData] = data
            addQuery[kSecAttrAccessible] = kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly
            SecItemAdd(addQuery as CFDictionary, nil)
        } else if status == errSecSuccess {
            SecItemUpdate(query as CFDictionary, attributes as CFDictionary)
        }
    }

    func get(_ key: String) -> String? {
        let query: [CFString: Any] = [
            kSecClass:            kSecClassGenericPassword,
            kSecAttrService:      service,
            kSecAttrAccount:      key,
            kSecReturnData:       true,
            kSecMatchLimit:       kSecMatchLimitOne,
        ]
        var result: AnyObject?
        let status = SecItemCopyMatching(query as CFDictionary, &result)
        guard status == errSecSuccess,
              let data = result as? Data,
              let value = String(data: data, encoding: .utf8)
        else { return nil }
        return value
    }

    func delete(_ key: String) {
        let query: [CFString: Any] = [
            kSecClass:       kSecClassGenericPassword,
            kSecAttrService: service,
            kSecAttrAccount: key,
        ]
        SecItemDelete(query as CFDictionary)
    }

    func has(_ key: String) -> Bool {
        let query: [CFString: Any] = [
            kSecClass:       kSecClassGenericPassword,
            kSecAttrService: service,
            kSecAttrAccount: key,
            kSecMatchLimit:  kSecMatchLimitOne,
        ]
        return SecItemCopyMatching(query as CFDictionary, nil) == errSecSuccess
    }

    func clearAll() {
        let query: [CFString: Any] = [
            kSecClass:       kSecClassGenericPassword,
            kSecAttrService: service,
        ]
        SecItemDelete(query as CFDictionary)
    }
}

// ─── Credential key constants — mirrors CredentialKey in SecureStore.kt ───────

enum CredentialKey {
    static let caldavServer                = "caldav_server"
    static let caldavUsername              = "caldav_username"
    static let caldavPassword              = "caldav_password"
    static let caldavAuthMethod            = "caldav_auth_method"
    static let caldavSelectedCalendars     = "caldav_selected_calendars"
    static let caldavDefaultEventCalendar  = "caldav_default_event_calendar"
    static let caldavDefaultTaskCalendar   = "caldav_default_task_calendar"
    static let giteaToken                  = "gitea_token"
    static let giteaInstance               = "gitea_instance"
    static let giteaRepos                  = "gitea_repos"
    static let pinHash                     = "biometric_pin_hash"
}
