package com.crossdashboard.app.ui.navigation

import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.Notes
import androidx.compose.material.icons.automirrored.outlined.Notes
import androidx.compose.material.icons.filled.*
import androidx.compose.material.icons.outlined.*
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.Icon
import androidx.compose.material3.Text
import androidx.compose.material3.adaptive.currentWindowAdaptiveInfo
import androidx.compose.material3.adaptive.navigationsuite.NavigationSuiteScaffold
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.hilt.navigation.compose.hiltViewModel
import androidx.navigation3.runtime.*
import androidx.navigation3.ui.NavDisplay
import androidx.window.core.layout.WindowSizeClass
import com.crossdashboard.app.ui.screen.dashboard.DashboardScreen
import com.crossdashboard.app.ui.screen.events.EventPropertySheet
import com.crossdashboard.app.ui.screen.events.EventsScreen
import com.crossdashboard.app.ui.screen.inbox.InboxScreen
import com.crossdashboard.app.ui.screen.issues.IssuesScreen
import com.crossdashboard.app.ui.screen.notes.NotesScreen
import com.crossdashboard.app.ui.screen.settings.SettingsScreen
import com.crossdashboard.app.ui.screen.memos.MemosScreen
import com.crossdashboard.app.ui.screen.tasks.TasksScreenContent
import com.crossdashboard.app.ui.screen.views.ViewsScreen
import com.crossdashboard.app.ui.viewmodel.NavigationViewModel

data class NavItem(
    val destination: Destination,
    val label: String,
    val selectedIcon: ImageVector,
    val unselectedIcon: ImageVector,
)

val ALL_NAV_ITEMS = listOf(
    NavItem(Destination.Dashboard, "Dashboard", Icons.Filled.Dashboard, Icons.Outlined.Dashboard),
    NavItem(Destination.Inbox, "Inbox", Icons.Filled.Inbox, Icons.Outlined.Inbox),
    NavItem(Destination.Events, "Events", Icons.Filled.CalendarMonth, Icons.Outlined.CalendarMonth),
    NavItem(Destination.Tasks, "Tasks", Icons.Filled.CheckBox, Icons.Outlined.CheckBoxOutlineBlank),
    NavItem(Destination.Notes, "Notes", Icons.AutoMirrored.Filled.Notes, Icons.AutoMirrored.Outlined.Notes),
    NavItem(Destination.Issues, "Issues", Icons.Filled.BugReport, Icons.Outlined.BugReport),
    NavItem(Destination.Views, "Views", Icons.Filled.ViewColumn, Icons.Outlined.ViewColumn),
    NavItem(Destination.Memos, "Capture", Icons.Filled.CameraAlt, Icons.Outlined.CameraAlt),
    NavItem(Destination.Settings, "Settings", Icons.Filled.Settings, Icons.Outlined.Settings),
)

@Composable
fun AppNavigation(
    modifier: Modifier = Modifier,
    visibleScreens: List<String>,
    pendingAction: String?,
    onActionConsumed: () -> Unit,
) {
    val navVm: NavigationViewModel = hiltViewModel()
    val colorResolver = navVm.colorResolver

    val backStack = rememberNavBackStack(Destination.Dashboard)
    val currentDestination = backStack.lastOrNull() ?: Destination.Dashboard

    // Handle notification-tap deep links that need to navigate to a specific item.
    LaunchedEffect(pendingAction) {
        when {
            pendingAction?.startsWith("events:") == true -> {
                val uid = pendingAction.removePrefix("events:")
                while (backStack.count() > 1) backStack.removeLastOrNull()
                if (backStack.lastOrNull()?.let { it::class } != Destination.Events::class) {
                    backStack += Destination.Events
                }
                backStack += Destination.EventDetail(uid)
                onActionConsumed()
            }
            pendingAction?.startsWith("tasks:") == true -> {
                val uid = pendingAction.removePrefix("tasks:")
                while (backStack.count() > 1) backStack.removeLastOrNull()
                if (backStack.lastOrNull()?.let { it::class } != Destination.Tasks::class) {
                    backStack += Destination.Tasks
                }
                backStack += Destination.TaskDetail(uid)
                onActionConsumed()
            }
        }
    }

    // Respect user-defined order: map visibleScreens (ordered) to NavItems, always append Settings.
    val settingsItem = ALL_NAV_ITEMS.first { it.destination.screenName() == "Settings" }
    val navItems = visibleScreens
        .filter { it != "Settings" }
        .mapNotNull { name -> ALL_NAV_ITEMS.find { it.destination.screenName() == name } }
        .plus(settingsItem)

    // On compact (phone bottom bar), cap visible items at 6 and put the rest in overflow.
    val windowInfo = currentWindowAdaptiveInfo()
    val isBottomBar = !windowInfo.windowSizeClass
        .isWidthAtLeastBreakpoint(WindowSizeClass.WIDTH_DP_MEDIUM_LOWER_BOUND)
    val maxBarItems = 6
    val mainItems    = if (isBottomBar && navItems.size > maxBarItems) navItems.take(maxBarItems - 1) else navItems
    val overflowItems = if (isBottomBar && navItems.size > maxBarItems) navItems.drop(maxBarItems - 1) else emptyList()
    var showOverflow by remember { mutableStateOf(false) }

    fun navigate(dest: Destination) {
        if (currentDestination::class != dest::class) {
            while (backStack.count() > 1) backStack.removeLastOrNull()
            if (backStack.lastOrNull()?.let { it::class } != dest::class) backStack += dest
        }
    }

    NavigationSuiteScaffold(
        modifier = modifier,
        navigationSuiteItems = {
            mainItems.forEach { item ->
                val isSelected = currentDestination::class == item.destination::class
                item(
                    selected = isSelected,
                    onClick = { navigate(item.destination) },
                    icon = {
                        Icon(
                            imageVector = if (isSelected) item.selectedIcon else item.unselectedIcon,
                            contentDescription = null,
                        )
                    },
                    label = { Text(item.label) },
                    modifier = Modifier.semantics(mergeDescendants = true) {
                        contentDescription = buildString {
                            append(item.label)
                            if (isSelected) append(", selected")
                        }
                    },
                )
            }

            // Overflow "More" button — only shown on compact when there are > maxBarItems items.
            if (overflowItems.isNotEmpty()) {
                val overflowSelected = overflowItems.any { it.destination::class == currentDestination::class }
                item(
                    selected = overflowSelected,
                    onClick = { showOverflow = true },
                    icon = {
                        androidx.compose.foundation.layout.Box {
                            Icon(
                                imageVector = if (overflowSelected) Icons.Filled.MoreHoriz else Icons.Outlined.MoreHoriz,
                                contentDescription = "More screens",
                            )
                            DropdownMenu(
                                expanded = showOverflow,
                                onDismissRequest = { showOverflow = false },
                            ) {
                                overflowItems.forEach { overflowItem ->
                                    val isSelected = currentDestination::class == overflowItem.destination::class
                                    DropdownMenuItem(
                                        text = { Text(overflowItem.label) },
                                        leadingIcon = {
                                            Icon(
                                                imageVector = if (isSelected) overflowItem.selectedIcon else overflowItem.unselectedIcon,
                                                contentDescription = null,
                                            )
                                        },
                                        onClick = {
                                            showOverflow = false
                                            navigate(overflowItem.destination)
                                        },
                                    )
                                }
                            }
                        }
                    },
                    label = { Text("More") },
                    modifier = Modifier.semantics(mergeDescendants = true) {
                        contentDescription = "More screens"
                    },
                )
            }
        },
    ) {
        NavDisplay(
            backStack = backStack,
            onBack = { backStack.removeLastOrNull() },
            entryProvider = entryProvider {
                entry<Destination.Dashboard> {
                    DashboardScreen(onNavigate = { backStack += it })
                }
                entry<Destination.Inbox> {
                    InboxScreen(onNavigate = { backStack += it })
                }
                entry<Destination.Events> {
                    EventsScreen(
                        onNavigate = { backStack += it },
                        colorResolver = colorResolver,
                    )
                }
                entry<Destination.Tasks> {
                    // Consume the pending action once Tasks is composed
                    LaunchedEffect(pendingAction) {
                        if (pendingAction != null) onActionConsumed()
                    }
                    TasksScreenContent(
                        onNavigate = { backStack += it },
                        pendingAction = pendingAction,
                        colorResolver = colorResolver,
                    )
                }
                entry<Destination.Notes> {
                    NotesScreen(colorResolver = colorResolver)
                }
                entry<Destination.Issues> {
                    IssuesScreen()
                }
                entry<Destination.Views> {
                    ViewsScreen(onNavigate = { backStack += it })
                }
                entry<Destination.Memos> {
                    MemosScreen(onNavigate = { backStack += it })
                }
                entry<Destination.Settings> {
                    SettingsScreen()
                }
                // Detail destinations — phone single-pane navigation
                entry<Destination.TaskDetail> { dest ->
                    TasksScreenContent(
                        onNavigate = { backStack += it },
                        pendingAction = null,
                        colorResolver = colorResolver,
                        initialUid = dest.uid,
                    )
                }
                entry<Destination.EventDetail> { dest ->
                    EventsScreen(
                        onNavigate = { backStack += it },
                        colorResolver = colorResolver,
                        initialUid = dest.uid,
                    )
                }
                entry<Destination.IssueDetail> {
                    IssuesScreen()
                }
                entry<Destination.NoteDetail> {
                    NotesScreen(colorResolver = colorResolver)
                }
                entry<Destination.MemoDetail> {
                    MemosScreen(onNavigate = { backStack += it })
                }
            },
        )
    }
}
