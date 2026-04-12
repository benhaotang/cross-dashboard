import SwiftUI
import CrossDashboardKit

/// Small colored circle indicating which calendar an event/task belongs to.
/// Color is resolved from the stored CalDavCalendar list or defaults to accent.
/// Mirrors CalendarColorResolver + CalendarColorDot on Android.
struct CalendarColorDot: View {

    let calendarHref: String?
    var size: CGFloat = 10

    @Environment(\.appContainer) private var container

    var body: some View {
        Circle()
            .fill(resolvedColor)
            .frame(width: size, height: size)
            .accessibilityHidden(true)
    }

    private var resolvedColor: Color {
        guard let href = calendarHref,
              let colorHex = container.preferences.cachedCalendars
                .first(where: { $0.href == href })?.color else {
            return .accentColor
        }
        return Color(hex: colorHex) ?? .accentColor
    }
}

// ─── AppPreferences cached calendars ─────────────────────────────────────────

extension AppPreferences {
    /// Decodes the stored CalDavCalendar list from Keychain for color resolution.
    var cachedCalendars: [CalDavCalendar] {
        guard let raw = KeychainStore.shared.get(CredentialKey.caldavSelectedCalendars),
              let data = raw.data(using: .utf8),
              let list = try? JSONDecoder().decode([CalDavCalendar].self, from: data) else {
            return []
        }
        return list
    }
}

// ─── Color hex init ───────────────────────────────────────────────────────────

extension Color {
    init?(hex: String) {
        let clean = hex.hasPrefix("#") ? String(hex.dropFirst()) : hex
        guard clean.count == 6,
              let value = UInt64(clean, radix: 16) else { return nil }
        let r = Double((value >> 16) & 0xFF) / 255
        let g = Double((value >> 8)  & 0xFF) / 255
        let b = Double(value          & 0xFF) / 255
        self.init(red: r, green: g, blue: b)
    }
}
