import SwiftUI
import CrossDashboardKit

/// Shared wrapper for all detail panes in the NavigationSplitView detail column.
/// Provides a consistent scrollable layout with a title header and content body.
/// Mirrors the PropertySheet + ReadView pattern on Android.
struct PropertyDetailShell<Content: View>: View {

    let title: String
    @ViewBuilder let content: () -> Content

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 0) {
                Text(title)
                    .font(.title2)
                    .fontWeight(.semibold)
                    .padding(.bottom, 16)

                VStack(alignment: .leading, spacing: 12) {
                    content()
                }
            }
            .padding(20)
            .frame(maxWidth: .infinity, alignment: .leading)
        }
        .navigationTitle(title)
    }
}
