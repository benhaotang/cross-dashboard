package com.crossdashboard.app.ui.navigation

import androidx.navigation3.runtime.NavKey
import kotlinx.serialization.Serializable

/**
 * Type-safe navigation destinations for Nav3.
 * All destinations implement NavKey and are @Serializable so they survive process death.
 */
@Serializable
sealed class Destination : NavKey {

    @Serializable data object Dashboard : Destination()
    @Serializable data object Inbox : Destination()
    @Serializable data object Events : Destination()
    @Serializable data object Tasks : Destination()
    @Serializable data object Notes : Destination()
    @Serializable data object Issues : Destination()
    @Serializable data object Views : Destination()
    @Serializable data object Memos : Destination()
    @Serializable data object Settings : Destination()

    // Detail destinations — used in phone (single-pane) mode
    @Serializable data class EventDetail(val uid: String) : Destination()
    @Serializable data class TaskDetail(val uid: String) : Destination()
    @Serializable data class NoteDetail(val uid: String) : Destination()
    @Serializable data class IssueDetail(val id: Long, val repo: String) : Destination()
    @Serializable data class MemoDetail(val name: String) : Destination()

    companion object {
        val navRoots = listOf(
            Dashboard, Inbox, Events, Tasks, Notes, Issues, Views, Memos, Settings,
        )
    }
}

/** Screen names used for visibility toggle — must match domain model ALL_SCREENS */
fun Destination.screenName(): String = when (this) {
    is Destination.Dashboard -> "Dashboard"
    is Destination.Inbox -> "Inbox"
    is Destination.Events -> "Events"
    is Destination.Tasks -> "Tasks"
    is Destination.Notes -> "Notes"
    is Destination.Issues -> "Issues"
    is Destination.Views -> "Views"
    is Destination.Memos -> "Memos"
    is Destination.Settings -> "Settings"
    else -> ""
}

fun Destination.label(): String = when (this) {
    is Destination.Dashboard -> "Dashboard"
    is Destination.Inbox -> "Inbox"
    is Destination.Events -> "Events"
    is Destination.Tasks -> "Tasks"
    is Destination.Notes -> "Notes"
    is Destination.Issues -> "Issues"
    is Destination.Views -> "Views"
    is Destination.Memos -> "Memos"
    is Destination.Settings -> "Settings"
    else -> ""
}
