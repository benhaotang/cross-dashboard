package com.crossdashboard.app.data.db.entity

import androidx.room.Entity
import androidx.room.PrimaryKey

@Entity(tableName = "events")
data class EventEntity(
    @PrimaryKey val uid: String,
    val summary: String,
    val startEpoch: Long,
    val endEpoch: Long,
    val description: String?,
    val location: String?,
    val calendarHref: String?,
    val etag: String?,
    val href: String?,
)

@Entity(tableName = "tasks")
data class TaskEntity(
    @PrimaryKey val uid: String,
    val summary: String,
    val description: String?,
    val status: String,             // TaskStatus.icalValue
    val priority: Int,
    val percentComplete: Int,
    val dueEpoch: Long?,
    val dtstartEpoch: Long?,
    val completedEpoch: Long?,
    val createdEpoch: Long,
    val lastModifiedEpoch: Long,
    val categoriesJson: String,     // JSON array of strings
    val location: String?,
    val parentUid: String?,
    val calendarHref: String?,
    val etag: String?,
    val href: String?,
)

@Entity(tableName = "notes")
data class NoteEntity(
    @PrimaryKey val uid: String,
    val summary: String,
    val body: String,
    val categoriesJson: String,     // JSON array
    val createdEpoch: Long,
    val lastModifiedEpoch: Long,
    val calendarHref: String?,
    val etag: String?,
    val href: String?,
)

@Entity(tableName = "issues")
data class IssueEntity(
    @PrimaryKey val id: Long,
    val number: Int,
    val title: String,
    val body: String,
    val state: String,
    val labelsJson: String,         // JSON array
    val assigneesJson: String,      // JSON array
    val createdAtEpoch: Long,
    val updatedAtEpoch: Long,
    val repository: String,
    val htmlUrl: String,
    val milestoneId: Long?,
    val milestoneTitle: String?,
    val milestoneDueOnEpoch: Long?,
)

@Entity(tableName = "daily_stats")
data class DailyStatsEntity(
    @PrimaryKey val date: String,   // ISO date "YYYY-MM-DD"
    val tasksCompleted: Int = 0,
    val pomodoroSessions: Int = 0,
    val issuesClosed: Int = 0,
)

@Entity(tableName = "memos")
data class MemoEntity(
    @PrimaryKey val name: String,   // "memos/{id}"
    val state: String,              // MemoState.name
    val content: String,
    val visibility: String,         // MemoVisibility.name
    val tagsJson: String,           // JSON array of strings
    val pinned: Boolean,
    val attachmentsJson: String,    // JSON array of MemosAttachment serialized
    val propertyHasLink: Boolean,
    val propertyHasTaskList: Boolean,
    val propertyHasIncompleteTasks: Boolean,
    val propertyTitle: String,
    val snippet: String,
    val createTimeEpoch: Long,
    val displayTimeEpoch: Long,
    val updateTimeEpoch: Long,
)
