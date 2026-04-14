package com.crossdashboard.app.ui.component

sealed interface ContentSegment {
    data class Markdown(val content: String) : ContentSegment
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
        if (!isDisplay) {
            val closingIndex = findClosingDelimiter(
                content = content,
                startIndex = index + 1,
                isDisplay = false,
            )
            if (closingIndex == -1) {
                markdown.append('$')
                index += 1
                continue
            }

            val inlineLatex = content.substring(index + 1, closingIndex)
            markdown.append(escapeInlineLatexForMarkdown(inlineLatex))
            index = closingIndex + 1
            continue
        }

        val closingIndex = findClosingDelimiter(
            content = content,
            startIndex = index + 2,
            isDisplay = true,
        )
        if (closingIndex == -1) {
            markdown.append('$')
            markdown.append('$')
            index += 2
            continue
        }

        val latex = content.substring(index + 2, closingIndex)
        if (latex.isBlank()) {
            markdown.append("$$")
            index += 2
            continue
        }

        flushMarkdown()
        segments += ContentSegment.DisplayMath(latex)
        index = closingIndex + 2
    }

    flushMarkdown()
    return segments
}

private fun escapeInlineLatexForMarkdown(latex: String): String {
    val escaped = buildString {
        latex.forEach { char ->
            if (char in MARKDOWN_SPECIALS) {
                append('\\')
            }
            append(char)
        }
    }
    return "\\$$escaped\\$"
}

private val MARKDOWN_SPECIALS = setOf('_', '*', '[', ']', '~')

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
