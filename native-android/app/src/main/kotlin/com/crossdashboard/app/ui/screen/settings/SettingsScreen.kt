package com.crossdashboard.app.ui.screen.settings

import android.app.WallpaperManager
import android.content.ComponentName
import android.content.Intent
import android.net.Uri
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.outlined.Notes
import androidx.compose.material.icons.outlined.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.text.input.VisualTransformation
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.window.Dialog
import androidx.hilt.navigation.compose.hiltViewModel
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.crossdashboard.app.BuildConfig
import com.crossdashboard.app.domain.model.*
import com.crossdashboard.app.background.*
import com.nextcloud.android.sso.AccountImporter

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun SettingsScreen(
    vm: SettingsViewModel = hiltViewModel(),
) {
    val state by vm.state.collectAsStateWithLifecycle()
    val context = LocalContext.current
    val snackbarHost = remember { SnackbarHostState() }

    // SSO launcher — step 1: shows the Android account picker filtered to Nextcloud accounts.
    // On success, AccountImporter.onActivityResult internally launches step 2
    // (SsoGrantPermissionActivity in the Nextcloud app) via Activity.startActivityForResult
    // with REQUEST_AUTH_TOKEN_SSO = 4243, which arrives in MainActivity.onActivityResult.
    // MainActivity forwards the final account name to SettingsViewModel via SsoResultBus.
    val ssoLauncher = rememberLauncherForActivityResult(ActivityResultContracts.StartActivityForResult()) { result ->
        try {
            AccountImporter.onActivityResult(
                AccountImporter.CHOOSE_ACCOUNT_SSO,
                result.resultCode,
                result.data,
                context as android.app.Activity,
            ) { _ -> /* step 2 result arrives via MainActivity */ }
        } catch (_: Exception) {
            vm.onSsoResult(null)  // user cancelled account picker
        }
    }

    // Show snackbar on info/error messages
    LaunchedEffect(state.infoMessage) {
        state.infoMessage?.let {
            snackbarHost.showSnackbar(it)
            vm.dismissInfo()
        }
    }
    LaunchedEffect(state.errorMessage) {
        state.errorMessage?.let {
            snackbarHost.showSnackbar(it)
            vm.dismissError()
        }
    }

    // PIN setup dialog — shown above the scaffold when enabling lock
    if (state.showPinSetupDialog) {
        PinSetupDialog(state = state, vm = vm)
    }

    Scaffold(
        topBar = { TopAppBar(title = { Text("Settings") }) },
        snackbarHost = { SnackbarHost(snackbarHost) },
    ) { padding ->
        LazyColumn(
            modifier = Modifier
                .fillMaxSize()
                .padding(padding),
            contentPadding = PaddingValues(bottom = 32.dp),
        ) {
            // ── CalDAV ───────────────────────────────────────────────────────
            item { SectionHeader("CalDAV Account") }
            item {
                CalDavSection(
                    state = state,
                    vm = vm,
                    onSsoLaunch = {
                        vm.buildSsoPickerIntent()?.let { ssoLauncher.launch(it) }
                    },
                    onOpenBrowser = { url ->
                        context.startActivity(Intent(Intent.ACTION_VIEW, Uri.parse(url)))
                    },
                )
            }

            // ── Calendar picker ─────────────────────────────────────────────
            // Show as soon as any auth path has succeeded (calendars may still be loading)
            if (state.availableCalendars.isNotEmpty() ||
                state.caldavConnectionStatus == CalDavConnectionStatus.SUCCESS ||
                state.loginFlowStatus == NextcloudFlowStatus.SUCCESS ||
                state.ssoAccountName != null
            ) {
                item { SectionHeader("Calendars") }
                item { CalendarPickerSection(state = state, vm = vm) }
            }

            // ── Gitea ────────────────────────────────────────────────────────
            item { SectionHeader("Gitea") }
            item { GiteaSection(state = state, vm = vm) }

            // ── Memos ─────────────────────────────────────────────────────────
            item { SectionHeader("Memos") }
            item { MemosSection(state = state, vm = vm) }

            // ── Appearance ───────────────────────────────────────────────────
            item { SectionHeader("Appearance") }
            item { AppearanceSection(state = state, vm = vm) }

            item { SectionHeader("Background") }
            item { BackgroundSection(state = state, vm = vm) }

            // ── Navigation ───────────────────────────────────────────────────
            item { SectionHeader("Navigation") }
            item { NavigationSection(state = state, vm = vm) }

            item { SectionHeader("Date & Time") }
            item { TimeZoneSection(state = state, vm = vm) }

            // ── Task input defaults ──────────────────────────────────────────
            item { SectionHeader("Task Input") }
            item { TaskDefaultsSection(state = state, vm = vm) }

            // ── Pomodoro ─────────────────────────────────────────────────────
            item { SectionHeader("Pomodoro Timer") }
            item { PomodoroSection(state = state, vm = vm) }

            // ── Notifications ────────────────────────────────────────────────
            item { SectionHeader("Notifications") }
            item { NotificationsSection(state = state, vm = vm) }

            // ── Widget sync ──────────────────────────────────────────────────
            item { SectionHeader("Background Sync") }
            item { WidgetSyncSection(state = state, vm = vm) }

            // ── Security ─────────────────────────────────────────────────────
            item { SectionHeader("Security") }
            item { SecuritySection(state = state, vm = vm) }

            // ── About ────────────────────────────────────────────────────────
            item { SectionHeader("About") }
            item { AboutSection() }
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun BackgroundSection(state: SettingsUiState, vm: SettingsViewModel) {
    val context = LocalContext.current
    val wallpaperInfo = WallpaperManager.getInstance(context).wallpaperInfo
    val isActive = wallpaperInfo?.component == ComponentName(context, DashboardWallpaperService::class.java)
    Column(
        modifier = Modifier.padding(horizontal = 16.dp, vertical = 8.dp),
        verticalArrangement = Arrangement.spacedBy(10.dp),
    ) {
        Text(
            if (isActive) "Cross-Dashboard live wallpaper is active" else "Cross-Dashboard live wallpaper is not active",
            style = MaterialTheme.typography.bodySmall,
            color = if (isActive) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.onSurfaceVariant,
        )
        Text("Preferred tablet orientation", style = MaterialTheme.typography.labelMedium)
        SingleChoiceSegmentedButtonRow(Modifier.fillMaxWidth()) {
            PreferredWallpaperOrientation.entries.forEachIndexed { index, value ->
                SegmentedButton(
                    shape = SegmentedButtonDefaults.itemShape(index, PreferredWallpaperOrientation.entries.size),
                    selected = state.wallpaperOrientation == value,
                    onClick = { vm.setWallpaperOrientation(value) },
                    label = { Text(value.name.lowercase().replaceFirstChar { it.uppercase() }) },
                )
            }
        }
        listOf(
            Triple("Standard", WallpaperProfile.STANDARD, state.backgroundStandard),
            Triple("Cover", WallpaperProfile.FOLD_COVER, state.backgroundCover),
            Triple("Opened", WallpaperProfile.FOLD_INNER, state.backgroundInner),
        ).forEach { (label, profile, template) ->
            Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
                Column(Modifier.weight(1f)) {
                    Text(label, style = MaterialTheme.typography.bodyMedium)
                    Text(
                        text = template?.let {
                            if (it.source == BackgroundSource.INBOX) {
                                "Inbox · ${it.inboxTypeFilter.lowercase()} · ${it.inboxDateFilter.lowercase()}"
                            } else {
                                "Views · ${it.viewsMode.lowercase()} · ${it.viewsTypeFilter.lowercase()} · ${it.viewsDateFilter.lowercase()}"
                            }
                        } ?: "Not configured",
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                }
                if (template != null) IconButton(onClick = { vm.clearBackground(profile) }) {
                    Icon(Icons.Outlined.Delete, contentDescription = "Clear $label background snapshot")
                }
            }
        }
        Button(onClick = {
            context.startActivity(Intent(WallpaperManager.ACTION_CHANGE_LIVE_WALLPAPER).apply {
                putExtra(WallpaperManager.EXTRA_LIVE_WALLPAPER_COMPONENT,
                    ComponentName(context, DashboardWallpaperService::class.java))
            })
        }) {
            Icon(Icons.Outlined.Wallpaper, contentDescription = null)
            Spacer(Modifier.width(8.dp))
            Text("Open wallpaper preview")
        }
    }
}

@Composable
private fun TimeZoneSection(state: SettingsUiState, vm: SettingsViewModel) {
    var input by remember(state.timeZoneOverride) { mutableStateOf(state.timeZoneOverride.orEmpty()) }
    Column(
        modifier = Modifier.padding(horizontal = 16.dp, vertical = 8.dp),
        verticalArrangement = Arrangement.spacedBy(8.dp),
    ) {
        Text(
            "Automatic uses the device timezone (${state.systemTimeZone}). Set an IANA timezone as a fallback for calendar data and display.",
            style = MaterialTheme.typography.bodySmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
        OutlinedTextField(
            value = input,
            onValueChange = { input = it },
            label = { Text("Timezone override") },
            placeholder = { Text("Europe/Berlin") },
            supportingText = { Text(if (state.timeZoneOverride == null) "Automatic" else "Active: ${state.timeZoneOverride}") },
            singleLine = true,
            modifier = Modifier.fillMaxWidth(),
        )
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Button(onClick = { vm.setTimeZoneOverride(input) }, enabled = input.isNotBlank()) {
                Text("Apply")
            }
            OutlinedButton(onClick = {
                input = ""
                vm.setTimeZoneOverride(null)
            }) {
                Text("Use system timezone")
            }
        }
    }
}

// ─── Section Header ───────────────────────────────────────────────────────────

@Composable
private fun SectionHeader(title: String) {
    Text(
        text = title.uppercase(),
        style = MaterialTheme.typography.labelSmall,
        color = MaterialTheme.colorScheme.primary,
        modifier = Modifier.padding(start = 16.dp, top = 20.dp, bottom = 4.dp),
    )
    HorizontalDivider(modifier = Modifier.padding(horizontal = 16.dp))
}

// ─── CalDAV section ───────────────────────────────────────────────────────────

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun CalDavSection(
    state: SettingsUiState,
    vm: SettingsViewModel,
    onSsoLaunch: () -> Unit,
    onOpenBrowser: (String) -> Unit,
) {
    Column(modifier = Modifier.padding(horizontal = 16.dp, vertical = 8.dp), verticalArrangement = Arrangement.spacedBy(10.dp)) {

        // Auth method selector
        Text("Sign-in method", style = MaterialTheme.typography.labelMedium)
        SingleChoiceSegmentedButtonRow(modifier = Modifier.fillMaxWidth()) {
            CalDavAuthMethod.entries.forEachIndexed { idx, method ->
                SegmentedButton(
                    shape = SegmentedButtonDefaults.itemShape(idx, CalDavAuthMethod.entries.size),
                    selected = state.authMethod == method,
                    onClick = { vm.setAuthMethod(method) },
                    label = {
                        Text(
                            when (method) {
                                CalDavAuthMethod.NEXTCLOUD_SSO -> "SSO"
                                CalDavAuthMethod.LOGIN_FLOW_V2 -> "Login Flow"
                                CalDavAuthMethod.MANUAL -> "Manual"
                            },
                            style = MaterialTheme.typography.labelSmall,
                        )
                    },
                )
            }
        }

        when (state.authMethod) {
            CalDavAuthMethod.NEXTCLOUD_SSO -> SsoSection(state, onSsoLaunch, vm::clearSsoAccount)
            CalDavAuthMethod.LOGIN_FLOW_V2 -> LoginFlowSection(state, vm, onOpenBrowser)
            CalDavAuthMethod.MANUAL -> ManualCalDavSection(state, vm)
        }

        // Connection status indicator
        if (state.caldavConnectionStatus != CalDavConnectionStatus.IDLE) {
            ConnectionStatusRow(state)
        }
    }
}

@Composable
private fun SsoSection(
    state: SettingsUiState,
    onSignIn: () -> Unit,
    onSignOut: () -> Unit,
) {
    if (!state.isNextcloudAppInstalled) {
        Text(
            "Nextcloud app is not installed. Install the Nextcloud app from F-Droid or Google Play to use SSO.",
            style = MaterialTheme.typography.bodySmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
        return
    }
    if (state.ssoAccountName != null) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.SpaceBetween,
        ) {
            Column {
                Text("Signed in via SSO", style = MaterialTheme.typography.bodyMedium, fontWeight = FontWeight.Medium)
                Text(state.ssoAccountName, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
            }
            OutlinedButton(onClick = onSignOut) { Text("Sign out") }
        }
    } else {
        Button(onClick = onSignIn, modifier = Modifier.fillMaxWidth()) {
            Icon(Icons.Outlined.AccountCircle, contentDescription = null, modifier = Modifier.size(18.dp))
            Spacer(Modifier.width(8.dp))
            Text("Sign in with Nextcloud")
        }
    }
}

@Composable
private fun LoginFlowSection(
    state: SettingsUiState,
    vm: SettingsViewModel,
    onOpenBrowser: (String) -> Unit,
) {
    var serverInput by remember { mutableStateOf(state.caldavServer) }

    when (state.loginFlowStatus) {
        NextcloudFlowStatus.IDLE, NextcloudFlowStatus.ERROR -> {
            OutlinedTextField(
                value = serverInput,
                onValueChange = { serverInput = it },
                label = { Text("Nextcloud server URL") },
                placeholder = { Text("https://cloud.example.com") },
                singleLine = true,
                modifier = Modifier.fillMaxWidth(),
            )
            if (state.loginFlowError != null) {
                Text(state.loginFlowError, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.error)
            }
            Button(
                onClick = { vm.startLoginFlow(serverInput) },
                modifier = Modifier.fillMaxWidth(),
                enabled = serverInput.isNotBlank(),
            ) {
                Text("Open Browser to Log In")
            }
        }
        NextcloudFlowStatus.INITIATING -> {
            CircularProgressIndicator(modifier = Modifier.size(24.dp))
            Text("Initiating login flow…", style = MaterialTheme.typography.bodySmall)
        }
        NextcloudFlowStatus.WAITING_BROWSER, NextcloudFlowStatus.POLLING -> {
            Text("Approve the login in your browser, then return here.", style = MaterialTheme.typography.bodyMedium)
            state.loginFlowUrl?.let { url ->
                TextButton(onClick = { onOpenBrowser(url) }) { Text("Re-open browser") }
            }
            Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                CircularProgressIndicator(modifier = Modifier.size(16.dp), strokeWidth = 2.dp)
                Text("Waiting for approval…", style = MaterialTheme.typography.bodySmall)
            }
            TextButton(onClick = vm::cancelLoginFlow) { Text("Cancel") }
        }
        NextcloudFlowStatus.SUCCESS -> {
            Row(
                modifier = Modifier.fillMaxWidth(),
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.SpaceBetween,
            ) {
                Row(
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.spacedBy(8.dp),
                    modifier = Modifier.weight(1f),
                ) {
                    Icon(Icons.Outlined.CheckCircle, contentDescription = null, tint = MaterialTheme.colorScheme.primary)
                    Column {
                        Text("Logged in", style = MaterialTheme.typography.bodyMedium, fontWeight = FontWeight.Medium)
                        Text(state.caldavUsername, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
                    }
                }
                OutlinedButton(onClick = vm::clearLoginFlowCredentials) { Text("Sign out") }
            }
        }
    }
}

@Composable
private fun ManualCalDavSection(state: SettingsUiState, vm: SettingsViewModel) {
    var showPassword by remember { mutableStateOf(false) }

    OutlinedTextField(
        value = state.caldavServer,
        onValueChange = vm::setCalDavServer,
        label = { Text("Server URL") },
        placeholder = { Text("https://dav.example.com") },
        singleLine = true,
        modifier = Modifier.fillMaxWidth(),
    )
    OutlinedTextField(
        value = state.caldavUsername,
        onValueChange = vm::setCalDavUsername,
        label = { Text("Username") },
        singleLine = true,
        modifier = Modifier.fillMaxWidth(),
    )
    OutlinedTextField(
        value = state.caldavPassword,
        onValueChange = vm::setCalDavPassword,
        label = { Text("Password") },
        singleLine = true,
        modifier = Modifier.fillMaxWidth(),
        visualTransformation = if (showPassword) VisualTransformation.None else PasswordVisualTransformation(),
        keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Password),
        trailingIcon = {
            IconButton(onClick = { showPassword = !showPassword }) {
                Icon(
                    if (showPassword) Icons.Outlined.VisibilityOff else Icons.Outlined.Visibility,
                    contentDescription = if (showPassword) "Hide" else "Show",
                )
            }
        },
    )
    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        OutlinedButton(
            onClick = vm::testCalDavConnection,
            enabled = state.caldavConnectionStatus != CalDavConnectionStatus.TESTING,
            modifier = Modifier.weight(1f),
        ) {
            if (state.caldavConnectionStatus == CalDavConnectionStatus.TESTING) {
                CircularProgressIndicator(modifier = Modifier.size(16.dp), strokeWidth = 2.dp)
            } else {
                Text("Test")
            }
        }
        Button(onClick = vm::saveCalDav, modifier = Modifier.weight(1f)) {
            Text("Save")
        }
    }
}

@Composable
private fun ConnectionStatusRow(state: SettingsUiState) {
    val (icon, color, text) = when (state.caldavConnectionStatus) {
        CalDavConnectionStatus.SUCCESS -> Triple(Icons.Outlined.CheckCircle, MaterialTheme.colorScheme.primary, state.caldavConnectionMessage)
        CalDavConnectionStatus.ERROR -> Triple(Icons.Outlined.Error, MaterialTheme.colorScheme.error, state.caldavConnectionMessage)
        else -> return
    }
    Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
        Icon(icon, contentDescription = null, tint = color, modifier = Modifier.size(16.dp))
        Text(text, style = MaterialTheme.typography.bodySmall, color = color)
    }
}

// ─── Calendar picker ──────────────────────────────────────────────────────────

@Composable
private fun CalendarPickerSection(state: SettingsUiState, vm: SettingsViewModel) {
    Column(modifier = Modifier.padding(horizontal = 16.dp, vertical = 8.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
        // Action row
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                TextButton(onClick = vm::selectAllCalendars) { Text("All") }
                TextButton(onClick = vm::deselectAllCalendars) { Text("None") }
            }
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp), verticalAlignment = Alignment.CenterVertically) {
                if (state.isLoadingCalendars) {
                    CircularProgressIndicator(modifier = Modifier.size(16.dp), strokeWidth = 2.dp)
                }
                IconButton(onClick = vm::refreshCalendars) {
                    Icon(Icons.Outlined.Refresh, contentDescription = "Refresh calendars")
                }
                Button(onClick = vm::saveCalendarSelection) { Text("Save") }
            }
        }

        // Calendar list
        state.availableCalendars.forEach { cal ->
            val isSelected = cal.href in state.selectedCalendarHrefs
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .clickable { vm.toggleCalendarSelection(cal.href) }
                    .padding(vertical = 6.dp),
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.spacedBy(10.dp),
            ) {
                // Color dot
                val dotColor = cal.color?.let {
                    runCatching {
                        val hex = it.trimStart('#')
                        Color(android.graphics.Color.parseColor("#$hex"))
                    }.getOrNull()
                } ?: MaterialTheme.colorScheme.primary

                Surface(
                    modifier = Modifier.size(12.dp),
                    shape = androidx.compose.foundation.shape.CircleShape,
                    color = dotColor,
                ) {}

                Column(modifier = Modifier.weight(1f)) {
                    Text(cal.displayName, style = MaterialTheme.typography.bodyMedium)
                    // Component badges
                    Row(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                        cal.components.forEach { comp ->
                            val label = when (comp) {
                                "VEVENT" -> "Events"
                                "VTODO" -> "Tasks"
                                "VJOURNAL" -> "Notes"
                                else -> comp
                            }
                            SuggestionChip(
                                onClick = {},
                                label = { Text(label, style = MaterialTheme.typography.labelSmall) },
                                modifier = Modifier.height(20.dp),
                            )
                        }
                    }
                }
                Checkbox(checked = isSelected, onCheckedChange = { vm.toggleCalendarSelection(cal.href) })
            }
        }

        // Default calendar selectors
        if (state.availableCalendars.isNotEmpty()) {
            Spacer(Modifier.height(8.dp))
            DefaultCalendarPicker(
                label = "Default Events calendar",
                calendars = state.availableCalendars.filter { "VEVENT" in it.components },
                selectedHref = state.defaultEventCalendar,
                onSelect = vm::setDefaultEventCalendar,
            )
            DefaultCalendarPicker(
                label = "Default Tasks calendar",
                calendars = state.availableCalendars.filter { "VTODO" in it.components },
                selectedHref = state.defaultTaskCalendar,
                onSelect = vm::setDefaultTaskCalendar,
            )
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun DefaultCalendarPicker(
    label: String,
    calendars: List<CalDavCalendar>,
    selectedHref: String?,
    onSelect: (String) -> Unit,
) {
    var expanded by remember { mutableStateOf(false) }
    val selected = calendars.firstOrNull { it.href == selectedHref }

    ExposedDropdownMenuBox(expanded = expanded, onExpandedChange = { expanded = it }) {
        OutlinedTextField(
            value = selected?.displayName ?: "Not set",
            onValueChange = {},
            readOnly = true,
            label = { Text(label) },
            trailingIcon = { ExposedDropdownMenuDefaults.TrailingIcon(expanded) },
            modifier = Modifier
                .fillMaxWidth()
                .menuAnchor(ExposedDropdownMenuAnchorType.PrimaryNotEditable),
        )
        ExposedDropdownMenu(expanded = expanded, onDismissRequest = { expanded = false }) {
            calendars.forEach { cal ->
                DropdownMenuItem(
                    text = { Text(cal.displayName) },
                    onClick = { onSelect(cal.href); expanded = false },
                )
            }
        }
    }
}

// ─── Gitea section ────────────────────────────────────────────────────────────

@Composable
private fun GiteaSection(state: SettingsUiState, vm: SettingsViewModel) {
    var showToken by remember { mutableStateOf(false) }

    Column(modifier = Modifier.padding(horizontal = 16.dp, vertical = 8.dp), verticalArrangement = Arrangement.spacedBy(10.dp)) {
        OutlinedTextField(
            value = state.giteaInstance,
            onValueChange = vm::setGiteaInstance,
            label = { Text("Gitea Instance URL") },
            placeholder = { Text("https://git.example.com") },
            singleLine = true,
            modifier = Modifier.fillMaxWidth(),
        )
        OutlinedTextField(
            value = state.giteaToken,
            onValueChange = vm::setGiteaToken,
            label = { Text("Access Token") },
            singleLine = true,
            modifier = Modifier.fillMaxWidth(),
            visualTransformation = if (showToken) VisualTransformation.None else PasswordVisualTransformation(),
            keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Password),
            trailingIcon = {
                IconButton(onClick = { showToken = !showToken }) {
                    Icon(
                        if (showToken) Icons.Outlined.VisibilityOff else Icons.Outlined.Visibility,
                        contentDescription = if (showToken) "Hide" else "Show",
                    )
                }
            },
        )
        OutlinedTextField(
            value = state.giteaRepos,
            onValueChange = vm::setGiteaRepos,
            label = { Text("Repositories") },
            placeholder = { Text("owner/repo, owner/other-repo") },
            singleLine = true,
            modifier = Modifier.fillMaxWidth(),
        )
        Button(onClick = vm::saveGitea, modifier = Modifier.fillMaxWidth()) {
            Text("Save Gitea Settings")
        }
    }
}

// ─── Memos section ────────────────────────────────────────────────────────────

@Composable
private fun MemosSection(state: SettingsUiState, vm: SettingsViewModel) {
    var showToken by remember { mutableStateOf(false) }

    Column(modifier = Modifier.padding(horizontal = 16.dp, vertical = 8.dp), verticalArrangement = Arrangement.spacedBy(10.dp)) {
        OutlinedTextField(
            value = state.memosHost,
            onValueChange = vm::setMemosHost,
            label = { Text("Memos Host URL") },
            placeholder = { Text("https://memos.example.com") },
            singleLine = true,
            modifier = Modifier.fillMaxWidth(),
        )
        OutlinedTextField(
            value = state.memosToken,
            onValueChange = vm::setMemosToken,
            label = { Text("Access Token") },
            singleLine = true,
            modifier = Modifier.fillMaxWidth(),
            visualTransformation = if (showToken) VisualTransformation.None else PasswordVisualTransformation(),
            keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Password),
            trailingIcon = {
                IconButton(onClick = { showToken = !showToken }) {
                    Icon(
                        if (showToken) Icons.Outlined.VisibilityOff else Icons.Outlined.Visibility,
                        contentDescription = if (showToken) "Hide token" else "Show token",
                    )
                }
            },
        )
        if (state.memosConnectionStatus != CalDavConnectionStatus.IDLE) {
            val (color, text) = when (state.memosConnectionStatus) {
                CalDavConnectionStatus.TESTING -> MaterialTheme.colorScheme.onSurfaceVariant to "Testing…"
                CalDavConnectionStatus.SUCCESS -> MaterialTheme.colorScheme.primary to state.memosConnectionMessage
                CalDavConnectionStatus.ERROR -> MaterialTheme.colorScheme.error to state.memosConnectionMessage
                else -> MaterialTheme.colorScheme.onSurfaceVariant to ""
            }
            Text(text, style = MaterialTheme.typography.bodySmall, color = color)
        }
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            OutlinedButton(
                onClick = vm::testMemosConnection,
                modifier = Modifier.weight(1f),
                enabled = state.memosConnectionStatus != CalDavConnectionStatus.TESTING,
            ) { Text("Test") }
            Button(onClick = vm::saveMemos, modifier = Modifier.weight(1f)) { Text("Save") }
        }
    }
}

// ─── Appearance section ───────────────────────────────────────────────────────

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun AppearanceSection(state: SettingsUiState, vm: SettingsViewModel) {
    Column(modifier = Modifier.padding(horizontal = 16.dp, vertical = 8.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
        Text("Theme", style = MaterialTheme.typography.labelMedium)
        SingleChoiceSegmentedButtonRow(modifier = Modifier.fillMaxWidth()) {
            ThemePreference.entries.forEachIndexed { idx, pref ->
                SegmentedButton(
                    shape = SegmentedButtonDefaults.itemShape(idx, ThemePreference.entries.size),
                    selected = state.theme == pref,
                    onClick = { vm.setTheme(pref) },
                    label = { Text(pref.name.lowercase().replaceFirstChar { it.uppercaseChar() }) },
                )
            }
        }
    }
}

// ─── Navigation section ───────────────────────────────────────────────────────

@Composable
private fun NavigationSection(state: SettingsUiState, vm: SettingsViewModel) {
    val visible = state.visibleScreens
    val hidden  = ALL_SCREENS.filter { it !in visible }

    fun iconFor(screen: String) = when (screen) {
        "Dashboard" -> Icons.Outlined.Dashboard
        "Inbox"     -> Icons.Outlined.Inbox
        "Events"    -> Icons.Outlined.CalendarMonth
        "Tasks"     -> Icons.Outlined.CheckBox
        "Notes"     -> Icons.AutoMirrored.Outlined.Notes
        "Issues"    -> Icons.Outlined.BugReport
        "Views"     -> Icons.Outlined.ViewColumn
        "Capture"   -> Icons.Outlined.CameraAlt
        else        -> Icons.Outlined.MoreHoriz
    }

    Column(modifier = Modifier.padding(horizontal = 16.dp, vertical = 4.dp)) {
        // ── Visible screens (ordered, with reorder buttons) ──────────────────
        if (visible.isNotEmpty()) {
            Text(
                "Visible screens",
                style = MaterialTheme.typography.labelSmall,
                color = MaterialTheme.colorScheme.primary,
                modifier = Modifier.padding(bottom = 4.dp),
            )
        }
        visible.forEachIndexed { index, screen ->
            Row(
                modifier = Modifier.fillMaxWidth().padding(vertical = 6.dp),
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.spacedBy(8.dp),
            ) {
                Icon(iconFor(screen), contentDescription = null, modifier = Modifier.size(20.dp), tint = MaterialTheme.colorScheme.onSurfaceVariant)
                Text(screen, modifier = Modifier.weight(1f), style = MaterialTheme.typography.bodyMedium)
                // Move up
                IconButton(
                    onClick = { if (index > 0) vm.moveVisibleScreen(index, index - 1) },
                    enabled = index > 0,
                    modifier = Modifier.size(32.dp),
                ) {
                    Icon(Icons.Outlined.KeyboardArrowUp, contentDescription = "Move $screen up", modifier = Modifier.size(18.dp))
                }
                // Move down
                IconButton(
                    onClick = { if (index < visible.lastIndex) vm.moveVisibleScreen(index, index + 1) },
                    enabled = index < visible.lastIndex,
                    modifier = Modifier.size(32.dp),
                ) {
                    Icon(Icons.Outlined.KeyboardArrowDown, contentDescription = "Move $screen down", modifier = Modifier.size(18.dp))
                }
                Switch(checked = true, onCheckedChange = { vm.toggleVisibleScreen(screen) })
            }
        }

        // ── Hidden screens ───────────────────────────────────────────────────
        if (hidden.isNotEmpty()) {
            Spacer(Modifier.height(8.dp))
            HorizontalDivider()
            Spacer(Modifier.height(8.dp))
            Text(
                "Hidden screens",
                style = MaterialTheme.typography.labelSmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
                modifier = Modifier.padding(bottom = 4.dp),
            )
        }
        hidden.forEach { screen ->
            Row(
                modifier = Modifier.fillMaxWidth().padding(vertical = 6.dp),
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.spacedBy(8.dp),
            ) {
                Icon(iconFor(screen), contentDescription = null, modifier = Modifier.size(20.dp), tint = MaterialTheme.colorScheme.onSurfaceVariant.copy(alpha = 0.5f))
                Text(screen, modifier = Modifier.weight(1f), style = MaterialTheme.typography.bodyMedium, color = MaterialTheme.colorScheme.onSurfaceVariant)
                Spacer(Modifier.width(68.dp)) // align with move buttons above
                Switch(checked = false, onCheckedChange = { vm.toggleVisibleScreen(screen) })
            }
        }
    }
}

// ─── Task defaults section ────────────────────────────────────────────────────

@Composable
private fun TaskDefaultsSection(state: SettingsUiState, vm: SettingsViewModel) {
    val d = state.taskDefaults
    Column(modifier = Modifier.padding(horizontal = 16.dp, vertical = 8.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
        Text("Time-of-day defaults for quick input parsing", style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
        HourInputRow("Morning hour", d.morningHour) { vm.setTaskDefaults(d.copy(morningHour = it)) }
        HourInputRow("Afternoon hour", d.afternoonHour) { vm.setTaskDefaults(d.copy(afternoonHour = it)) }
        HourInputRow("Night hour", d.nightHour) { vm.setTaskDefaults(d.copy(nightHour = it)) }
        HourInputRow("Default hour", d.defaultHour) { vm.setTaskDefaults(d.copy(defaultHour = it)) }
    }
}

@Composable
private fun HourInputRow(label: String, current: Int, onUpdate: (Int) -> Unit) {
    var text by remember(current) { mutableStateOf(current.toString()) }
    OutlinedTextField(
        value = text,
        onValueChange = { v ->
            text = v
            v.toIntOrNull()?.coerceIn(0, 23)?.let { onUpdate(it) }
        },
        label = { Text(label) },
        singleLine = true,
        keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
        suffix = { Text(":00") },
        modifier = Modifier.fillMaxWidth(),
    )
}

// ─── Pomodoro section ─────────────────────────────────────────────────────────

@Composable
private fun PomodoroSection(state: SettingsUiState, vm: SettingsViewModel) {
    val p = state.pomodoroSettings
    Column(modifier = Modifier.padding(horizontal = 16.dp, vertical = 8.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
        MinutesInputRow("Work minutes", p.workMinutes) { vm.setPomodoroSettings(p.copy(workMinutes = it)) }
        MinutesInputRow("Short break minutes", p.shortBreakMinutes) { vm.setPomodoroSettings(p.copy(shortBreakMinutes = it)) }
        MinutesInputRow("Long break minutes", p.longBreakMinutes) { vm.setPomodoroSettings(p.copy(longBreakMinutes = it)) }
        MinutesInputRow("Sessions until long break", p.sessionsUntilLongBreak, max = 10) { vm.setPomodoroSettings(p.copy(sessionsUntilLongBreak = it)) }
    }
}

@Composable
private fun MinutesInputRow(label: String, current: Int, max: Int = 120, onUpdate: (Int) -> Unit) {
    var text by remember(current) { mutableStateOf(current.toString()) }
    OutlinedTextField(
        value = text,
        onValueChange = { v ->
            text = v
            v.toIntOrNull()?.coerceIn(1, max)?.let { onUpdate(it) }
        },
        label = { Text(label) },
        singleLine = true,
        keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
        modifier = Modifier.fillMaxWidth(),
    )
}

// ─── Notifications section ────────────────────────────────────────────────────

@Composable
private fun NotificationsSection(state: SettingsUiState, vm: SettingsViewModel) {
    Column(modifier = Modifier.padding(horizontal = 16.dp, vertical = 4.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.SpaceBetween,
        ) {
            Column {
                Text("Event reminders", style = MaterialTheme.typography.bodyMedium)
                Text("Notifications before events start", style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
            }
            Switch(
                checked = state.notificationsEnabled,
                onCheckedChange = { vm.setNotifications(it, state.notificationMinutes) },
            )
        }
        if (state.notificationsEnabled) {
            var minText by remember(state.notificationMinutes) { mutableStateOf(state.notificationMinutes.toString()) }
            OutlinedTextField(
                value = minText,
                onValueChange = { v ->
                    minText = v
                    v.toIntOrNull()?.coerceIn(1, 120)?.let { vm.setNotifications(true, it) }
                },
                label = { Text("Minutes before event") },
                singleLine = true,
                keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
                modifier = Modifier.fillMaxWidth(),
            )
        }
    }
}

// ─── Widget / Background Sync section ────────────────────────────────────────

@Composable
private fun WidgetSyncSection(state: SettingsUiState, vm: SettingsViewModel) {
    Column(modifier = Modifier.padding(horizontal = 16.dp, vertical = 8.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
        Text(
            "Sync interval: ${state.widgetSyncInterval} min",
            style = MaterialTheme.typography.bodyMedium,
        )
        Slider(
            value = state.widgetSyncInterval.toFloat(),
            onValueChange = {},
            onValueChangeFinished = { },
            valueRange = 15f..240f,
            steps = 14,
            modifier = Modifier.fillMaxWidth(),
        )
        // Use proper step slider via snapped value
        val sliderSteps = listOf(15, 30, 60, 90, 120, 180, 240)
        Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            sliderSteps.forEach { min ->
                FilterChip(
                    selected = state.widgetSyncInterval == min,
                    onClick = { vm.setWidgetSyncInterval(min) },
                    label = { Text("${min}m") },
                )
            }
        }
        Button(
            onClick = vm::syncNow,
            modifier = Modifier.fillMaxWidth(),
            enabled = !state.isSyncingNow,
        ) {
            if (state.isSyncingNow) {
                CircularProgressIndicator(modifier = Modifier.size(16.dp), strokeWidth = 2.dp)
                Spacer(Modifier.width(8.dp))
            }
            Text("Sync Now")
        }
    }
}

// ─── Security section ─────────────────────────────────────────────────────────

@Composable
private fun SecuritySection(state: SettingsUiState, vm: SettingsViewModel) {
    Column(modifier = Modifier.padding(horizontal = 16.dp, vertical = 4.dp)) {

        // ── Row 1: App PIN lock ───────────────────────────────────────────────
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(vertical = 8.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.SpaceBetween,
        ) {
            Column(modifier = Modifier.weight(1f)) {
                Text("App PIN lock", style = MaterialTheme.typography.bodyMedium)
                Text(
                    "Require a custom PIN to open the app",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
            Switch(
                checked = state.pinLockEnabled,
                onCheckedChange = { enabled ->
                    if (enabled) vm.requestEnableLock() else vm.disableLock()
                },
            )
        }

        // ── Row 2: System credential (biometric) ─────────────────────────────
        // Grayed out unless the PIN lock is active
        val credentialAlpha = if (state.pinLockEnabled) 1f else 0.38f
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(vertical = 8.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.SpaceBetween,
        ) {
            Column(modifier = Modifier.weight(1f)) {
                Text(
                    "Use biometric / device credential",
                    style = MaterialTheme.typography.bodyMedium,
                    color = MaterialTheme.colorScheme.onSurface.copy(alpha = credentialAlpha),
                )
                Text(
                    "Show a biometric button on the PIN screen as an alternative to typing your PIN",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant.copy(alpha = credentialAlpha),
                )
            }
            Switch(
                checked = state.systemCredentialEnabled,
                onCheckedChange = { vm.setSystemCredential(it) },
                enabled = state.pinLockEnabled,
            )
        }
    }
}

// ─── PIN setup dialog ─────────────────────────────────────────────────────────

@Composable
private fun PinSetupDialog(state: SettingsUiState, vm: SettingsViewModel) {
    Dialog(onDismissRequest = vm::cancelPinSetup) {
        Surface(
            shape = MaterialTheme.shapes.extraLarge,
            color = MaterialTheme.colorScheme.surface,
            tonalElevation = 6.dp,
        ) {
            Column(
                modifier = Modifier
                    .padding(horizontal = 28.dp, vertical = 24.dp)
                    .fillMaxWidth(),
                horizontalAlignment = Alignment.CenterHorizontally,
                verticalArrangement = Arrangement.spacedBy(20.dp),
            ) {
                Text(
                    text = if (state.pinSetupStep == PinSetupStep.ENTER_NEW) "Set PIN" else "Confirm PIN",
                    style = MaterialTheme.typography.headlineSmall,
                    fontWeight = FontWeight.SemiBold,
                )

                Text(
                    text = if (state.pinSetupStep == PinSetupStep.ENTER_NEW)
                        "Choose a 6-digit PIN to lock the app"
                    else
                        "Re-enter your PIN to confirm",
                    style = MaterialTheme.typography.bodyMedium,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                    textAlign = TextAlign.Center,
                )

                // PIN entry dots
                Row(horizontalArrangement = Arrangement.spacedBy(14.dp)) {
                    val hasError = state.pinSetupError != null
                    val dotColor = if (hasError) MaterialTheme.colorScheme.error
                                   else MaterialTheme.colorScheme.primary
                    repeat(6) { index ->
                        Box(
                            modifier = Modifier
                                .size(14.dp)
                                .clip(CircleShape)
                                .background(
                                    if (index < state.pinSetupCurrent.length) dotColor
                                    else dotColor.copy(alpha = 0.2f)
                                )
                        )
                    }
                }

                if (state.pinSetupError != null) {
                    Text(
                        text = state.pinSetupError,
                        color = MaterialTheme.colorScheme.error,
                        style = MaterialTheme.typography.bodySmall,
                        textAlign = TextAlign.Center,
                    )
                }

                PinSetupNumpad(onDigit = vm::onPinSetupDigit, onDelete = vm::onPinSetupDelete)

                TextButton(
                    onClick = vm::cancelPinSetup,
                    modifier = Modifier.align(Alignment.End),
                ) {
                    Text("Cancel")
                }
            }
        }
    }
}

@Composable
private fun PinSetupNumpad(onDigit: (String) -> Unit, onDelete: () -> Unit) {
    val rows = listOf(listOf("1", "2", "3"), listOf("4", "5", "6"), listOf("7", "8", "9"))
    Column(
        verticalArrangement = Arrangement.spacedBy(10.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
    ) {
        rows.forEach { row ->
            Row(horizontalArrangement = Arrangement.spacedBy(20.dp)) {
                row.forEach { digit ->
                    PinSetupKey(label = digit, onClick = { onDigit(digit) })
                }
            }
        }
        Row(horizontalArrangement = Arrangement.spacedBy(20.dp)) {
            Spacer(modifier = Modifier.size(60.dp))
            PinSetupKey(label = "0", onClick = { onDigit("0") })
            PinSetupKey(label = "⌫", onClick = onDelete)
        }
    }
}

@Composable
private fun PinSetupKey(label: String, onClick: () -> Unit) {
    Box(
        modifier = Modifier
            .size(60.dp)
            .clip(CircleShape)
            .background(MaterialTheme.colorScheme.surfaceVariant)
            .clickable(onClick = onClick),
        contentAlignment = Alignment.Center,
    ) {
        Text(
            text = label,
            fontSize = 22.sp,
            fontWeight = FontWeight.Medium,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
    }
}

// ─── About section ────────────────────────────────────────────────────────────

@Composable
private fun AboutSection() {
    val version = try { BuildConfig.VERSION_NAME } catch (_: Exception) { "dev" }
    ListItem(
        leadingContent = { Icon(Icons.Outlined.Info, contentDescription = null) },
        headlineContent = { Text("Cross-Dashboard v$version") },
        supportingContent = { Text("Native Android — Jetpack Compose") },
    )
}
