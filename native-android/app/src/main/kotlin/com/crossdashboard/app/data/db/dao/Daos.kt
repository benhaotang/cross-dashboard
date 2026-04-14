package com.crossdashboard.app.data.db.dao

import androidx.room.*
import com.crossdashboard.app.data.db.entity.*
import kotlinx.coroutines.flow.Flow

@Dao
interface EventDao {
    @Query("SELECT * FROM events ORDER BY startEpoch ASC")
    fun observeAll(): Flow<List<EventEntity>>

    @Query("SELECT * FROM events WHERE startEpoch >= :fromEpoch AND startEpoch <= :toEpoch ORDER BY startEpoch ASC")
    fun observeRange(fromEpoch: Long, toEpoch: Long): Flow<List<EventEntity>>

    @Query("SELECT * FROM events WHERE startEpoch >= :nowEpoch ORDER BY startEpoch ASC LIMIT :limit")
    suspend fun getUpcoming(nowEpoch: Long, limit: Int): List<EventEntity>

    @Upsert
    suspend fun upsertAll(events: List<EventEntity>)

    @Query("DELETE FROM events")
    suspend fun deleteAll()
}

@Dao
interface TaskDao {
    @Query("SELECT * FROM tasks ORDER BY dueEpoch ASC NULLS LAST, createdEpoch DESC")
    fun observeAll(): Flow<List<TaskEntity>>

    @Query("SELECT * FROM tasks WHERE status != 'COMPLETED' AND status != 'CANCELLED' ORDER BY dueEpoch ASC NULLS LAST")
    fun observeActive(): Flow<List<TaskEntity>>

    @Query("SELECT * FROM tasks WHERE status = 'COMPLETED' ORDER BY completedEpoch DESC")
    fun observeCompleted(): Flow<List<TaskEntity>>

    @Query("SELECT * FROM tasks WHERE uid = :uid")
    suspend fun getByUid(uid: String): TaskEntity?

    @Query("SELECT * FROM tasks WHERE parentUid = :parentUid")
    fun observeSubtasks(parentUid: String): Flow<List<TaskEntity>>

    @Query("SELECT * FROM tasks WHERE dueEpoch <= :deadlineEpoch AND status != 'COMPLETED' AND status != 'CANCELLED' ORDER BY dueEpoch ASC LIMIT :limit")
    suspend fun getDueSoon(deadlineEpoch: Long, limit: Int): List<TaskEntity>

    @Upsert
    suspend fun upsertAll(tasks: List<TaskEntity>)

    @Query("DELETE FROM tasks")
    suspend fun deleteAll()

    @Query("DELETE FROM tasks WHERE uid = :uid")
    suspend fun deleteByUid(uid: String)
}

@Dao
interface NoteDao {
    @Query("SELECT * FROM notes ORDER BY lastModifiedEpoch DESC")
    fun observeAll(): Flow<List<NoteEntity>>

    @Query("SELECT * FROM notes WHERE uid = :uid")
    suspend fun getByUid(uid: String): NoteEntity?

    @Upsert
    suspend fun upsertAll(notes: List<NoteEntity>)

    @Query("DELETE FROM notes")
    suspend fun deleteAll()

    @Query("DELETE FROM notes WHERE uid = :uid")
    suspend fun deleteByUid(uid: String)
}

@Dao
interface IssueDao {
    @Query("SELECT * FROM issues ORDER BY updatedAtEpoch DESC")
    fun observeAll(): Flow<List<IssueEntity>>

    @Query("SELECT * FROM issues WHERE state = 'open' ORDER BY updatedAtEpoch DESC")
    fun observeOpen(): Flow<List<IssueEntity>>

    @Query("SELECT * FROM issues WHERE id = :id")
    suspend fun getById(id: Long): IssueEntity?

    @Upsert
    suspend fun upsertAll(issues: List<IssueEntity>)

    @Query("DELETE FROM issues")
    suspend fun deleteAll()
}

@Dao
interface DailyStatsDao {
    @Query("SELECT * FROM daily_stats WHERE date = :date")
    suspend fun getForDate(date: String): DailyStatsEntity?

    @Query("SELECT * FROM daily_stats WHERE date >= :startDate ORDER BY date ASC")
    suspend fun getRange(startDate: String): List<DailyStatsEntity>

    @Upsert
    suspend fun upsert(stats: DailyStatsEntity)

    @Transaction
    suspend fun increment(date: String, field: StatField) {
        val existing = getForDate(date) ?: DailyStatsEntity(date = date)
        val updated = when (field) {
            StatField.TASKS_COMPLETED -> existing.copy(tasksCompleted = existing.tasksCompleted + 1)
            StatField.POMODORO_SESSIONS -> existing.copy(pomodoroSessions = existing.pomodoroSessions + 1)
            StatField.ISSUES_CLOSED -> existing.copy(issuesClosed = existing.issuesClosed + 1)
        }
        upsert(updated)
    }
}

enum class StatField { TASKS_COMPLETED, POMODORO_SESSIONS, ISSUES_CLOSED }

@Dao
interface MemosDao {
    @Query("SELECT * FROM memos WHERE state = 'NORMAL' ORDER BY displayTimeEpoch DESC")
    fun observeNormal(): Flow<List<com.crossdashboard.app.data.db.entity.MemoEntity>>

    @Query("SELECT * FROM memos WHERE state = 'ARCHIVED' ORDER BY displayTimeEpoch DESC")
    fun observeArchived(): Flow<List<com.crossdashboard.app.data.db.entity.MemoEntity>>

    @Query("SELECT * FROM memos ORDER BY displayTimeEpoch DESC")
    fun observeAll(): Flow<List<com.crossdashboard.app.data.db.entity.MemoEntity>>

    @Query("SELECT * FROM memos WHERE name = :name")
    suspend fun getByName(name: String): com.crossdashboard.app.data.db.entity.MemoEntity?

    @Query("SELECT * FROM memos WHERE displayTimeEpoch >= :fromEpoch AND displayTimeEpoch <= :toEpoch ORDER BY displayTimeEpoch DESC")
    fun observeRange(fromEpoch: Long, toEpoch: Long): Flow<List<com.crossdashboard.app.data.db.entity.MemoEntity>>

    @Upsert
    suspend fun upsertAll(memos: List<com.crossdashboard.app.data.db.entity.MemoEntity>)

    @Upsert
    suspend fun upsert(memo: com.crossdashboard.app.data.db.entity.MemoEntity)

    @Query("DELETE FROM memos")
    suspend fun deleteAll()

    @Query("DELETE FROM memos WHERE name = :name")
    suspend fun deleteByName(name: String)
}
