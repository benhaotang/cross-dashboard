package com.crossdashboard.app.ui.screen.memos

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.itemsIndexed
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.crossdashboard.app.domain.model.ParsedTask
import java.time.ZoneId
import java.time.format.DateTimeFormatter

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun ExtractTasksConfirmSheet(
    parsedTasks: List<ParsedTask>,
    onDismiss: () -> Unit,
    vm: MemosViewModel,
) {
    val dueFmt = DateTimeFormatter.ofPattern("MMM d").withZone(ZoneId.systemDefault())
    val selected = remember { mutableStateListOf(*Array(parsedTasks.size) { true }) }

    ModalBottomSheet(onDismissRequest = onDismiss) {
        Column(
            modifier = Modifier
                .padding(horizontal = 16.dp)
                .padding(bottom = 24.dp),
            verticalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            Text("Extract Tasks", style = MaterialTheme.typography.titleMedium)
            if (parsedTasks.isEmpty()) {
                Text("No incomplete tasks found (- [ ] …) in this memo.", color = MaterialTheme.colorScheme.onSurfaceVariant)
            } else {
                Text("Select tasks to create:", style = MaterialTheme.typography.labelMedium)
                LazyColumn(modifier = Modifier.heightIn(max = 320.dp)) {
                    itemsIndexed(parsedTasks) { idx, task ->
                        Row(verticalAlignment = Alignment.CenterVertically) {
                            Checkbox(
                                checked = selected[idx],
                                onCheckedChange = { selected[idx] = it },
                            )
                            Column(modifier = Modifier.padding(start = 4.dp)) {
                                Text(task.summary, style = MaterialTheme.typography.bodyMedium)
                                val meta = buildString {
                                    if (task.priority > 0) append("P${task.priority} ")
                                    task.due?.let { append(dueFmt.format(it)) }
                                    if (task.categories.isNotEmpty()) append(" #${task.categories.joinToString(" #")}")
                                }
                                if (meta.isNotBlank()) {
                                    Text(meta.trim(), style = MaterialTheme.typography.labelSmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
                                }
                            }
                        }
                    }
                }
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.spacedBy(8.dp, Alignment.End),
                ) {
                    OutlinedButton(onClick = onDismiss) { Text("Cancel") }
                    Button(
                        onClick = {
                            val toCreate = parsedTasks.filterIndexed { idx, _ -> selected.getOrElse(idx) { false } }
                            toCreate.forEach { parsed ->
                                // Use TaskRepository via ViewModel to create each task
                                vm.createTaskFromParsed(parsed)
                            }
                            onDismiss()
                        },
                        enabled = selected.any { it },
                    ) { Text("Create ${selected.count { it }} task${if (selected.count { it } != 1) "s" else ""}") }
                }
            }
        }
    }
}
