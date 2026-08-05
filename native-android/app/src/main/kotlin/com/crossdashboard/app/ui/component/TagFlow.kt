package com.crossdashboard.app.ui.component

import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.ExperimentalLayoutApi
import androidx.compose.foundation.layout.FlowRow
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp

private val TIME_TAG = Regex("""^\d+(m|h)$""", RegexOption.IGNORE_CASE)
private val COVEY_TAGS = setOf("do", "delay", "delegate", "eliminate")

private enum class TagKind { TIME, MAGIC, ORDINARY }

/**
 * Compact, display-only tags that use all available width before wrapping.
 * Keeping the pills non-interactive lets a containing card own the click target.
 */
@OptIn(ExperimentalLayoutApi::class)
@Composable
fun TagFlow(
    tags: List<String>,
    modifier: Modifier = Modifier,
    magicTags: Collection<String> = emptyList(),
) {
    val normalizedMagicTags = (magicTags + COVEY_TAGS)
        .mapTo(mutableSetOf()) { it.trim().trimStart('#').lowercase() }
    val visibleTags = tags
        .map { it.trim().trimStart('#') }
        .filter { it.isNotEmpty() }
        .distinctBy { it.lowercase() }

    FlowRow(
        modifier = modifier,
        horizontalArrangement = Arrangement.spacedBy(4.dp),
        verticalArrangement = Arrangement.spacedBy(4.dp),
    ) {
        visibleTags.forEach { tag ->
            val kind = when {
                TIME_TAG.matches(tag) -> TagKind.TIME
                tag.lowercase() in normalizedMagicTags -> TagKind.MAGIC
                else -> TagKind.ORDINARY
            }
            TagPill(tag = tag, kind = kind)
        }
    }
}

@Composable
private fun TagPill(tag: String, kind: TagKind) {
    val colors = MaterialTheme.colorScheme
    val container: Color
    val content: Color
    val outline: Color
    when (kind) {
        TagKind.TIME -> {
            container = colors.tertiaryContainer.copy(alpha = 0.72f)
            content = colors.onTertiaryContainer
            outline = colors.tertiary.copy(alpha = 0.42f)
        }
        TagKind.MAGIC -> {
            container = colors.primaryContainer.copy(alpha = 0.72f)
            content = colors.onPrimaryContainer
            outline = colors.primary.copy(alpha = 0.42f)
        }
        TagKind.ORDINARY -> {
            container = colors.surfaceVariant.copy(alpha = 0.72f)
            content = colors.onSurfaceVariant
            outline = colors.outlineVariant
        }
    }

    Surface(
        shape = CircleShape,
        color = container,
        contentColor = content,
        border = BorderStroke(1.dp, outline),
    ) {
        Text(
            text = "#$tag",
            style = MaterialTheme.typography.labelSmall,
            modifier = Modifier.padding(horizontal = 8.dp, vertical = 3.dp),
        )
    }
}
