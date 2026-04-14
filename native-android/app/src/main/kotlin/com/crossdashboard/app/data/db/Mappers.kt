package com.crossdashboard.app.data.db

import com.crossdashboard.app.data.db.entity.*
import com.crossdashboard.app.domain.model.*
import kotlinx.serialization.encodeToString
import kotlinx.serialization.json.Json
import java.time.Instant

private val json = Json { ignoreUnknownKeys = true }

// ─── CalendarEvent ────────────────────────────────────────────────────────────

fun CalendarEvent.toEntity() = EventEntity(
    uid = uid,
    summary = summary,
    startEpoch = start.toEpochMilli(),
    endEpoch = end.toEpochMilli(),
    description = description,
    location = location,
    calendarHref = calendarHref,
    etag = etag,
    href = href,
)

fun EventEntity.toDomain() = CalendarEvent(
    uid = uid,
    summary = summary,
    start = Instant.ofEpochMilli(startEpoch),
    end = Instant.ofEpochMilli(endEpoch),
    description = description,
    location = location,
    calendarHref = calendarHref,
    etag = etag,
    href = href,
)

// ─── CalDavTask ───────────────────────────────────────────────────────────────

fun CalDavTask.toEntity() = TaskEntity(
    uid = uid,
    summary = summary,
    description = description,
    status = status.icalValue,
    priority = priority,
    percentComplete = percentComplete,
    dueEpoch = due?.toEpochMilli(),
    dtstartEpoch = dtstart?.toEpochMilli(),
    completedEpoch = completed?.toEpochMilli(),
    createdEpoch = created.toEpochMilli(),
    lastModifiedEpoch = lastModified.toEpochMilli(),
    categoriesJson = json.encodeToString(categories),
    location = location,
    parentUid = parentUid,
    calendarHref = calendarHref,
    etag = etag,
    href = href,
)

fun TaskEntity.toDomain() = CalDavTask(
    uid = uid,
    summary = summary,
    description = description,
    status = TaskStatus.fromIcal(status),
    priority = priority,
    percentComplete = percentComplete,
    due = dueEpoch?.let { Instant.ofEpochMilli(it) },
    dtstart = dtstartEpoch?.let { Instant.ofEpochMilli(it) },
    completed = completedEpoch?.let { Instant.ofEpochMilli(it) },
    created = Instant.ofEpochMilli(createdEpoch),
    lastModified = Instant.ofEpochMilli(lastModifiedEpoch),
    categories = runCatching { json.decodeFromString<List<String>>(categoriesJson) }.getOrDefault(emptyList()),
    location = location,
    parentUid = parentUid,
    calendarHref = calendarHref,
    etag = etag,
    href = href,
)

// ─── Note ─────────────────────────────────────────────────────────────────────

fun Note.toEntity() = NoteEntity(
    uid = uid,
    summary = summary,
    body = body,
    categoriesJson = json.encodeToString(categories),
    createdEpoch = created.toEpochMilli(),
    lastModifiedEpoch = lastModified.toEpochMilli(),
    calendarHref = calendarHref,
    etag = etag,
    href = href,
)

fun NoteEntity.toDomain() = Note(
    uid = uid,
    summary = summary,
    body = body,
    categories = runCatching { json.decodeFromString<List<String>>(categoriesJson) }.getOrDefault(emptyList()),
    created = Instant.ofEpochMilli(createdEpoch),
    lastModified = Instant.ofEpochMilli(lastModifiedEpoch),
    calendarHref = calendarHref,
    etag = etag,
    href = href,
)

// ─── GiteaIssue ───────────────────────────────────────────────────────────────

fun GiteaIssue.toEntity() = IssueEntity(
    id = id,
    number = number,
    title = title,
    body = body,
    state = state,
    labelsJson = json.encodeToString(labels),
    assigneesJson = json.encodeToString(assignees),
    createdAtEpoch = createdAt.toEpochMilli(),
    updatedAtEpoch = updatedAt.toEpochMilli(),
    repository = repository,
    htmlUrl = htmlUrl,
)

fun IssueEntity.toDomain() = GiteaIssue(
    id = id,
    number = number,
    title = title,
    body = body,
    state = state,
    labels = runCatching { json.decodeFromString<List<String>>(labelsJson) }.getOrDefault(emptyList()),
    assignees = runCatching { json.decodeFromString<List<String>>(assigneesJson) }.getOrDefault(emptyList()),
    createdAt = Instant.ofEpochMilli(createdAtEpoch),
    updatedAt = Instant.ofEpochMilli(updatedAtEpoch),
    repository = repository,
    htmlUrl = htmlUrl,
)

// ─── MemosMemo ────────────────────────────────────────────────────────────────

fun MemosMemo.toEntity() = MemoEntity(
    name = name,
    state = state.name,
    content = content,
    visibility = visibility.name,
    tagsJson = json.encodeToString(tags),
    pinned = pinned,
    attachmentsJson = json.encodeToString(attachments),
    propertyHasLink = property.hasLink,
    propertyHasTaskList = property.hasTaskList,
    propertyHasIncompleteTasks = property.hasIncompleteTasks,
    propertyTitle = property.title,
    snippet = snippet,
    createTimeEpoch = createTime.toEpochMilli(),
    displayTimeEpoch = displayTime.toEpochMilli(),
    updateTimeEpoch = updateTime.toEpochMilli(),
)

fun MemoEntity.toDomain() = MemosMemo(
    name = name,
    state = runCatching { MemoState.valueOf(state) }.getOrDefault(MemoState.NORMAL),
    content = content,
    visibility = runCatching { MemoVisibility.valueOf(visibility) }.getOrDefault(MemoVisibility.PRIVATE),
    tags = runCatching { json.decodeFromString<List<String>>(tagsJson) }.getOrDefault(emptyList()),
    pinned = pinned,
    attachments = runCatching { json.decodeFromString<List<MemosAttachment>>(attachmentsJson) }.getOrDefault(emptyList()),
    property = MemoProperty(
        hasLink = propertyHasLink,
        hasTaskList = propertyHasTaskList,
        hasIncompleteTasks = propertyHasIncompleteTasks,
        title = propertyTitle,
    ),
    snippet = snippet,
    createTime = Instant.ofEpochMilli(createTimeEpoch),
    displayTime = Instant.ofEpochMilli(displayTimeEpoch),
    updateTime = Instant.ofEpochMilli(updateTimeEpoch),
)
