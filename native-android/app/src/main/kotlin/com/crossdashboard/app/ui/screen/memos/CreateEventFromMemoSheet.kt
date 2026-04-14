package com.crossdashboard.app.ui.screen.memos

import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.crossdashboard.app.data.prefs.CredentialKey
import com.crossdashboard.app.data.prefs.SecureStore
import com.crossdashboard.app.domain.model.MemosMemo
import dagger.hilt.android.lifecycle.HiltViewModel
import java.time.Instant
import java.time.ZoneId
import java.time.format.DateTimeFormatter
import javax.inject.Inject
import androidx.hilt.navigation.compose.hiltViewModel
import androidx.lifecycle.ViewModel

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun CreateEventFromMemoSheet(
    memo: MemosMemo,
    memosHost: String,
    seedDate: Instant?,
    onDismiss: () -> Unit,
    vm: MemosViewModel,
) {
    val memoUrl = if (memosHost.isNotBlank()) "$memosHost/${memo.name}" else ""
    val firstLine = memo.content.lines().firstOrNull { it.isNotBlank() }?.take(80) ?: memo.snippet.take(80)

    var summary by remember { mutableStateOf(memo.property.title.ifBlank { firstLine }) }
    var description by remember { mutableStateOf(if (memoUrl.isNotBlank()) "$memoUrl\n\n${memo.snippet}" else memo.snippet) }
    var startHour by remember { mutableStateOf(seedDate?.let { java.time.LocalDateTime.ofInstant(it, ZoneId.systemDefault()).hour } ?: 10) }
    var durationHours by remember { mutableStateOf(1) }
    var calendarHref by remember { mutableStateOf("") }

    // We'd normally load calendars from a repo; reuse the stored default
    val sharedVM: EventCalendarPickerVm = hiltViewModel()
    val calendars by sharedVM.calendars.collectAsState()

    val dateFmt = DateTimeFormatter.ofPattern("MMM d, yyyy").withZone(ZoneId.systemDefault())

    ModalBottomSheet(onDismissRequest = onDismiss, contentWindowInsets = { WindowInsets.ime }) {
        Column(
            modifier = Modifier
                .padding(horizontal = 16.dp)
                .padding(bottom = 24.dp),
            verticalArrangement = Arrangement.spacedBy(12.dp),
        ) {
            Text("Create Event from Memo", style = MaterialTheme.typography.titleMedium)
            if (seedDate != null) {
                Text("Date: ${dateFmt.format(seedDate)}", style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
            }

            OutlinedTextField(value = summary, onValueChange = { summary = it }, label = { Text("Summary") }, modifier = Modifier.fillMaxWidth(), singleLine = true)
            OutlinedTextField(value = description, onValueChange = { description = it }, label = { Text("Description") }, modifier = Modifier.fillMaxWidth(), minLines = 2)

            // Start hour slider
            Text("Start hour: $startHour:00", style = MaterialTheme.typography.labelMedium)
            Slider(value = startHour.toFloat(), onValueChange = { startHour = it.toInt() }, valueRange = 0f..23f, steps = 22)

            // Duration
            Text("Duration: $durationHours hour${if (durationHours != 1) "s" else ""}", style = MaterialTheme.typography.labelMedium)
            Slider(value = durationHours.toFloat(), onValueChange = { durationHours = it.toInt().coerceAtLeast(1) }, valueRange = 1f..8f, steps = 6)

            // Calendar selector
            if (calendars.isNotEmpty()) {
                var expanded by remember { mutableStateOf(false) }
                ExposedDropdownMenuBox(expanded = expanded, onExpandedChange = { expanded = it }) {
                    OutlinedTextField(
                        value = calendars.firstOrNull { it.first == calendarHref }?.second ?: "Select calendar",
                        onValueChange = {},
                        readOnly = true,
                        label = { Text("Calendar") },
                        trailingIcon = { ExposedDropdownMenuDefaults.TrailingIcon(expanded) },
                        modifier = Modifier.menuAnchor(MenuAnchorType.PrimaryNotEditable).fillMaxWidth(),
                    )
                    ExposedDropdownMenu(expanded = expanded, onDismissRequest = { expanded = false }) {
                        calendars.forEach { (href, name) ->
                            DropdownMenuItem(text = { Text(name) }, onClick = { calendarHref = href; expanded = false })
                        }
                    }
                }
            }

            Row(horizontalArrangement = Arrangement.spacedBy(8.dp, androidx.compose.ui.Alignment.End), modifier = Modifier.fillMaxWidth()) {
                OutlinedButton(onClick = onDismiss) { Text("Cancel") }
                Button(
                    onClick = {
                        val base = seedDate ?: Instant.now()
                        val zone = ZoneId.systemDefault()
                        val localDate = java.time.LocalDateTime.ofInstant(base, zone).toLocalDate()
                        val start = localDate.atTime(startHour, 0).atZone(zone).toInstant()
                        val end = start.plusSeconds(durationHours * 3600L)
                        vm.createEventFromMemo(summary, description, start, end, calendarHref)
                        onDismiss()
                    },
                    enabled = summary.isNotBlank() && calendarHref.isNotBlank(),
                ) { Text("Create") }
            }
        }
    }
}

/** Lightweight helper ViewModel to vend available calendars. */
@HiltViewModel
class EventCalendarPickerVm @Inject constructor(
    secureStore: SecureStore,
) : ViewModel() {
    private val _calendars = kotlinx.coroutines.flow.MutableStateFlow<List<Pair<String, String>>>(emptyList())
    val calendars: kotlinx.coroutines.flow.StateFlow<List<Pair<String, String>>> = _calendars

    init {
        val raw = secureStore.get(CredentialKey.CALDAV_SELECTED_CALENDARS)
        if (raw != null) {
            runCatching {
                kotlinx.serialization.json.Json.decodeFromString<List<com.crossdashboard.app.domain.model.CalDavCalendar>>(raw)
                    .map { it.href to it.displayName }
            }.getOrNull()?.let { _calendars.value = it }
        }
    }
}
