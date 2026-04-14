package com.crossdashboard.app.ui.screen.memos

import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.crossdashboard.app.domain.model.MemosMemo
import com.crossdashboard.app.ui.component.MarkdownText
import java.time.ZoneId
import java.time.format.DateTimeFormatter

@Composable
fun MemoCommentItem(
    comment: MemosMemo,
    modifier: Modifier = Modifier,
) {
    val dateFormatter = DateTimeFormatter.ofPattern("MMM d, HH:mm").withZone(ZoneId.systemDefault())

    Column(
        modifier = modifier
            .fillMaxWidth()
            .padding(horizontal = 16.dp, vertical = 8.dp),
        verticalArrangement = Arrangement.spacedBy(4.dp),
    ) {
        Text(
            dateFormatter.format(comment.displayTime),
            style = MaterialTheme.typography.labelSmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
        Surface(
            color = MaterialTheme.colorScheme.surfaceVariant,
            shape = MaterialTheme.shapes.medium,
            modifier = Modifier.fillMaxWidth(),
        ) {
            MarkdownText(
                content = comment.content,
                modifier = Modifier.padding(12.dp),
            )
        }
    }
}
