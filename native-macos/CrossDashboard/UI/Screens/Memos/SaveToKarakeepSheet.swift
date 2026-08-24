import SwiftUI

struct SaveToKarakeepSheet: View {
    let urls: [URL]
    @Bindable var viewModel: MemosViewModel
    @Environment(\.dismiss) private var dismiss
    @State private var selectedFolderId = ""

    var body: some View {
        VStack(alignment: .leading, spacing: 16) {
            Label(
                urls.count == 1 ? "Save link to Karakeep" : "Save \(urls.count) links to Karakeep",
                systemImage: "bookmark.badge.plus"
            )
            .font(.title2)

            VStack(alignment: .leading, spacing: 4) {
                ForEach(urls.prefix(3), id: \.self) { url in
                    Text(url.absoluteString).lineLimit(1).truncationMode(.middle)
                }
                if urls.count > 3 { Text("+\(urls.count - 3) more").foregroundStyle(.secondary) }
            }
            .font(.callout)

            Picker("Folder", selection: $selectedFolderId) {
                Text("No folder").tag("")
                ForEach(viewModel.karakeepFolders) { folder in
                    Text(folderLabel(folder)).tag(folder.id)
                }
            }
            .disabled(viewModel.karakeepFoldersLoading)

            if let error = viewModel.error {
                Text(error).font(.caption).foregroundStyle(.red)
            }

            HStack {
                if viewModel.karakeepFoldersLoading { ProgressView().controlSize(.small) }
                Spacer()
                Button("Cancel", role: .cancel) { dismiss() }
                Button("Save") {
                    Task {
                        if await viewModel.saveToKarakeep(
                            urls: urls,
                            folderId: selectedFolderId.isEmpty ? nil : selectedFolderId
                        ) { dismiss() }
                    }
                }
                .keyboardShortcut(.defaultAction)
                .disabled(viewModel.karakeepFoldersLoading || viewModel.karakeepSaving)
            }
        }
        .padding(24)
        .frame(width: 440)
        .task {
            viewModel.clearError()
            await viewModel.loadKarakeepFolders()
        }
    }

    private func folderLabel(_ folder: KarakeepFolder) -> String {
        let byId = Dictionary(uniqueKeysWithValues: viewModel.karakeepFolders.map { ($0.id, $0) })
        var names = [folder.name]
        var parentId = folder.parentId
        var visited: Set<String> = [folder.id]
        while let id = parentId, visited.insert(id).inserted, let parent = byId[id] {
            names.insert(parent.name, at: 0)
            parentId = parent.parentId
        }
        return names.joined(separator: " / ")
    }
}
