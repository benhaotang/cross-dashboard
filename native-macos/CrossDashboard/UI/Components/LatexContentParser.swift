import Foundation

enum ContentSegment: Hashable {
    case markdown(String)
    case displayMath(String)
}

func parseLatex(_ content: String) -> [ContentSegment] {
    guard !content.isEmpty else { return [] }

    var segments: [ContentSegment] = []
    var markdown = ""
    var index = content.startIndex

    func flushMarkdown() {
        guard !markdown.isEmpty else { return }
        segments.append(.markdown(markdown))
        markdown.removeAll(keepingCapacity: true)
    }

    while index < content.endIndex {
        let current = content[index]

        if current == "\\",
           let nextIndex = content.index(index, offsetBy: 1, limitedBy: content.endIndex),
           nextIndex < content.endIndex,
           content[nextIndex] == "$" {
            markdown.append("$")
            index = content.index(after: nextIndex)
            continue
        }

        guard current == "$" else {
            markdown.append(current)
            index = content.index(after: index)
            continue
        }

        let secondIndex = content.index(index, offsetBy: 1, limitedBy: content.endIndex)
        let isDisplay = secondIndex.map { $0 < content.endIndex && content[$0] == "$" } ?? false

        if !isDisplay {
            guard let closingIndex = findClosingDelimiter(
                in: content,
                start: content.index(after: index),
                isDisplay: false
            ) else {
                markdown.append("$")
                index = content.index(after: index)
                continue
            }

            let inlineLatex = String(content[content.index(after: index)..<closingIndex])
            markdown.append(escapeInlineLatexForMarkdown(inlineLatex))
            index = content.index(after: closingIndex)
            continue
        }

        let searchStart = content.index(after: secondIndex!)
        guard let closingIndex = findClosingDelimiter(
            in: content,
            start: searchStart,
            isDisplay: true
        ) else {
            markdown.append("$")
            markdown.append("$")
            index = content.index(after: secondIndex!)
            continue
        }

        let latexStart = searchStart
        let latex = String(content[latexStart..<closingIndex])
        guard !latex.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty else {
            markdown.append("$$")
            index = searchStart
            continue
        }

        flushMarkdown()
        segments.append(.displayMath(latex))
        index = content.index(after: content.index(after: closingIndex))
    }

    flushMarkdown()
    return segments
}

private func findClosingDelimiter(
    in content: String,
    start: String.Index,
    isDisplay: Bool
) -> String.Index? {
    var index = start

    while index < content.endIndex {
        let current = content[index]

        if current == "\\",
           let nextIndex = content.index(index, offsetBy: 1, limitedBy: content.endIndex),
           nextIndex < content.endIndex,
           content[nextIndex] == "$" {
            index = content.index(after: nextIndex)
            continue
        }

        guard current == "$" else {
            index = content.index(after: index)
            continue
        }

        let secondIndex = content.index(index, offsetBy: 1, limitedBy: content.endIndex)
        if isDisplay {
            if let secondIndex, secondIndex < content.endIndex, content[secondIndex] == "$" {
                return index
            }
            index = content.index(after: index)
            continue
        }

        if let secondIndex, secondIndex < content.endIndex, content[secondIndex] == "$" {
            index = content.index(after: secondIndex)
            continue
        }
        return index
    }

    return nil
}

private func escapeInlineLatexForMarkdown(_ latex: String) -> String {
    let escaped = latex.reduce(into: "") { partialResult, character in
        if markdownSpecials.contains(character) {
            partialResult.append("\\")
        }
        partialResult.append(character)
    }
    return "\\$\(escaped)\\$"
}

private let markdownSpecials: Set<Character> = ["_", "*", "[", "]", "~"]
