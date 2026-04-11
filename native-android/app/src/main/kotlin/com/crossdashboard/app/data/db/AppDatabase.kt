package com.crossdashboard.app.data.db

import androidx.room.Database
import androidx.room.RoomDatabase
import com.crossdashboard.app.data.db.dao.*
import com.crossdashboard.app.data.db.entity.*

@Database(
    entities = [
        EventEntity::class,
        TaskEntity::class,
        NoteEntity::class,
        IssueEntity::class,
        DailyStatsEntity::class,
    ],
    version = 1,
    exportSchema = true,
)
abstract class AppDatabase : RoomDatabase() {
    abstract fun eventDao(): EventDao
    abstract fun taskDao(): TaskDao
    abstract fun noteDao(): NoteDao
    abstract fun issueDao(): IssueDao
    abstract fun dailyStatsDao(): DailyStatsDao
}
