package com.crossdashboard.app.ui.component

sealed interface ContentSegment {
    data class Markdown(val content: String) : ContentSegment
    data class InlineMath(val latex: String) : ContentSegment
    data class DisplayMath(val latex: String) : ContentSegment
}

fun parseLatex(content: String): List<ContentSegment> {
    if (content.isEmpty()) return emptyList()

    val segments = mutableListOf<ContentSegment>()
    val markdown = StringBuilder()
    var index = 0

    fun flushMarkdown() {
        if (markdown.isNotEmpty()) {
            segments += ContentSegment.Markdown(markdown.toString())
            markdown.setLength(0)
        }
    }

    while (index < content.length) {
        val current = content[index]
        if (current == '\\' && index + 1 < content.length && content[index + 1] == '$') {
            markdown.append('$')
            index += 2
            continue
        }

        if (current != '$') {
            markdown.append(current)
            index += 1
            continue
        }

        val isDisplay = index + 1 < content.length && content[index + 1] == '$'
        val delimiterLength = if (isDisplay) 2 else 1
        val closingIndex = findClosingDelimiter(
            content = content,
            startIndex = index + delimiterLength,
            isDisplay = isDisplay,
        )

        if (closingIndex == -1) {
            markdown.append('$')
            if (isDisplay) {
                markdown.append('$')
                index += 2
            } else {
                index += 1
            }
            continue
        }

        val latex = content.substring(index + delimiterLength, closingIndex)
        if (latex.isBlank()) {
            markdown.append(if (isDisplay) "$$" else "$")
            index += delimiterLength
            continue
        }

        flushMarkdown()
        segments += if (isDisplay) {
            ContentSegment.DisplayMath(latex)
        } else {
            ContentSegment.InlineMath(latex)
        }
        index = closingIndex + delimiterLength
    }

    flushMarkdown()
    return segments
}

private fun findClosingDelimiter(
    content: String,
    startIndex: Int,
    isDisplay: Boolean,
): Int {
    var index = startIndex

    while (index < content.length) {
        if (content[index] == '\\' && index + 1 < content.length && content[index + 1] == '$') {
            index += 2
            continue
        }

        if (content[index] != '$') {
            index += 1
            continue
        }

        if (isDisplay) {
            if (index + 1 < content.length && content[index + 1] == '$') {
                return index
            }
            index += 1
            continue
        }

        if (index + 1 < content.length && content[index + 1] == '$') {
            index += 2
            continue
        }
        return index
    }

    return -1
}
