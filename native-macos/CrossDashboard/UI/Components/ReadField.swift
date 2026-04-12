import SwiftUI
import CrossDashboardKit

/// A labelled read-only field used in detail panes.
/// The label is rendered as small-caps secondary text above the value.
/// Mirrors ReadField composable on Android.
struct ReadField<Value: View>: View {

    let label: String
    @ViewBuilder let value: () -> Value

    var body: some View {
        VStack(alignment: .leading, spacing: 3) {
            Text(label.uppercased())
                .font(.caption2)
                .fontWeight(.semibold)
                .foregroundStyle(.secondary)
                .kerning(0.5)

            value()
                .font(.callout)
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .accessibilityElement(children: .combine)
        .accessibilityLabel("\(label)")
    }
}

/// Convenience overload for plain string values.
extension ReadField where Value == Text {
    init(label: String, text: String) {
        self.label = label
        self.value = { Text(text) }
    }
}
