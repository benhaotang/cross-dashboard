package com.crossdashboard.app.ui.navigation

import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.*
import androidx.compose.material.icons.outlined.*
import androidx.compose.material3.adaptive.navigationsuite.NavigationSuiteScaffold
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.hilt.navigation.compose.hiltViewModel
import androidx.navigation3.runtime.*
import androidx.navigation3.ui.NavDisplay
import com.crossdashboard.app.ui.screen.dashboard.DashboardScreen
import com.crossdashboard.app.ui.screen.events.EventPropertySheet
import com.crossdashboard.app.ui.screen.events.EventsScreen
import com.crossdashboard.app.ui.screen.inbox.InboxScreen
import com.crossdashboard.app.ui.screen.issues.IssuesScreen
import com.crossdashboard.app.ui.screen.notes.NotesScreen
import com.crossdashboard.app.ui.screen.settings.SettingsScreen
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
    NavItem(Destination.Notes, "Notes", Icons.Filled.Notes, Icons.Outlined.Notes),
    NavItem(Destination.Issues, "Issues", Icons.Filled.BugReport, Icons.Outlined.BugReport),
    NavItem(Destination.Views, "Views", Icons.Filled.ViewColumn, Icons.Outlined.ViewColumn),
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

    val navItems = ALL_NAV_ITEMS.filter { item ->
        item.destination.screenName() == "Settings" ||
            visibleScreens.contains(item.destination.screenName())
    }

    NavigationSuiteScaffold(
        modifier = modifier,
        navigationSuiteItems = {
            navItems.forEach { item ->
                val isSelected = currentDestination::class == item.destination::class
                item(
                    selected = isSelected,
                    onClick = {
                        if (!isSelected) {
                            while (backStack.count() > 1) backStack.removeLastOrNull()
                            if (backStack.lastOrNull()?.let { it::class } != item.destination::class) {
                                backStack += item.destination
                            }
                        }
                    },
                    icon = {
                        androidx.compose.material3.Icon(
                            imageVector = if (isSelected) item.selectedIcon else item.unselectedIcon,
                            contentDescription = null,
                        )
                    },
                    label = { androidx.compose.material3.Text(item.label) },
                    modifier = Modifier.semantics(mergeDescendants = true) {
                        contentDescription = buildString {
                            append(item.label)
                            if (isSelected) append(", selected")
                        }
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
            },
        )
    }
}
