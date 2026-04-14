package com.crossdashboard.app.ui.component

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.MaterialTheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.unit.takeOrElse
import io.github.darriousliu.katex.core.MTMathView
import io.github.darriousliu.katex.core.MTMathViewMode
import io.github.darriousliu.katex.core.MTTextAlignment
import com.mikepenz.markdown.coil3.Coil3ImageTransformerImpl
import com.mikepenz.markdown.m3.Markdown
import com.mikepenz.markdown.m3.markdownColor
import com.mikepenz.markdown.m3.markdownTypography

/**
 * Renders [content] as GitHub-Flavoured Markdown using Material 3 colour tokens.
 *
 * Only use this in **read-only** contexts. Edit forms should use plain [TextField].
 * Images embedded in markdown are loaded via Coil 3 (the same instance used elsewhere
 * in the app) so network/disk caching behaves consistently.
 */
@Composable
fun MarkdownText(
    content: String,
    modifier: Modifier = Modifier,
) {
    val colors = markdownColor(
        text = MaterialTheme.colorScheme.onSurface,
        codeBackground = MaterialTheme.colorScheme.surfaceVariant,
        inlineCodeBackground = MaterialTheme.colorScheme.surfaceVariant,
        dividerColor = MaterialTheme.colorScheme.outlineVariant,
        tableBackground = MaterialTheme.colorScheme.surfaceContainerLow,
    )
    val typography = markdownTypography(
        h1 = MaterialTheme.typography.headlineLarge,
        h2 = MaterialTheme.typography.headlineMedium,
        h3 = MaterialTheme.typography.headlineSmall,
        h4 = MaterialTheme.typography.titleLarge,
        h5 = MaterialTheme.typography.titleMedium,
        h6 = MaterialTheme.typography.titleSmall,
        text = MaterialTheme.typography.bodyMedium,
        paragraph = MaterialTheme.typography.bodyMedium,
        ordered = MaterialTheme.typography.bodyMedium,
        bullet = MaterialTheme.typography.bodyMedium,
        list = MaterialTheme.typography.bodyMedium,
        quote = MaterialTheme.typography.bodyMedium,
        code = MaterialTheme.typography.labelMedium,
    )

    Column(
        modifier = modifier,
        verticalArrangement = Arrangement.spacedBy(4.dp),
    ) {
        parseLatex(content).forEach { segment ->
            when (segment) {
                is ContentSegment.Markdown -> Markdown(
                    content = segment.content,
                    colors = colors,
                    typography = typography,
                    imageTransformer = Coil3ImageTransformerImpl,
                    modifier = Modifier.fillMaxWidth(),
                )

                is ContentSegment.DisplayMath -> LatexText(
                    latex = segment.latex,
                    mode = MTMathViewMode.KMTMathViewModeDisplay,
                    centered = true,
                )
            }
        }
    }
}

@Composable
private fun LatexText(
    latex: String,
    mode: MTMathViewMode,
    centered: Boolean = false,
) {
    Box(
        modifier = Modifier
            .fillMaxWidth()
            .padding(vertical = if (centered) 8.dp else 0.dp),
        contentAlignment = if (centered) Alignment.Center else Alignment.CenterStart,
    ) {
        MTMathView(
            latex = latex,
            modifier = Modifier.fillMaxWidth(),
            fontSize = if (centered) {
                MaterialTheme.typography.titleMedium.fontSize.takeOrElse { 20.sp }
            } else {
                MaterialTheme.typography.bodyLarge.fontSize.takeOrElse { 18.sp }
            },
            textColor = MaterialTheme.colorScheme.onSurface,
            font = null,
            mode = mode,
            textAlignment = if (centered) {
                MTTextAlignment.KMTTextAlignmentCenter
            } else {
                MTTextAlignment.KMTTextAlignmentLeft
            },
            displayErrorInline = true,
            errorFontSize = MaterialTheme.typography.bodySmall.fontSize.takeOrElse { 14.sp },
        )
    }
}
