package com.crossdashboard.app.data.repository

import android.util.Log
import com.crossdashboard.app.data.db.Mappers
import com.crossdashboard.app.data.db.dao.MemosDao
import com.crossdashboard.app.data.db.toDomain
import com.crossdashboard.app.data.db.toEntity
import com.crossdashboard.app.data.network.MemosClient
import com.crossdashboard.app.data.prefs.CredentialKey
import com.crossdashboard.app.data.prefs.SecureStore
import com.crossdashboard.app.domain.model.*
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.withContext
import javax.inject.Inject
import javax.inject.Singleton

@Singleton
class MemoRepository @Inject constructor(
    private val dao: MemosDao,
    private val client: MemosClient,
    private val secureStore: SecureStore,
) {
    private val tag = "MemoRepository"

    // ─── Observe ──────────────────────────────────────────────────────────────

    val allMemos: Flow<List<MemosMemo>> = dao.observeAll().map { list ->
        list.map { it.toDomain() }
    }

    val normalMemos: Flow<List<MemosMemo>> = dao.observeNormal().map { list ->
        list.map { it.toDomain() }
    }

    val archivedMemos: Flow<List<MemosMemo>> = dao.observeArchived().map { list ->
        list.map { it.toDomain() }
    }

    // ─── Sync ─────────────────────────────────────────────────────────────────

    /** Pages through all memos (NORMAL + ARCHIVED) and replaces the local cache. */
    suspend fun syncMemos() = withContext(Dispatchers.IO) {
        if (client.baseUrl() == null) return@withContext
        val allFetched = mutableListOf<MemosMemo>()

        for (state in listOf(MemoState.NORMAL, MemoState.ARCHIVED)) {
            var pageToken: String? = null
            do {
                val (page, next) = client.listMemos(pageToken = pageToken, state = state)
                allFetched += page
                pageToken = next
            } while (pageToken != null)
        }

        if (allFetched.isNotEmpty()) {
            runCatching {
                dao.deleteAll()
                dao.upsertAll(allFetched.map { it.toEntity() })
            }.onFailure { e -> Log.e(tag, "syncMemos DB write failed", e) }
        }
    }

    // ─── Create ───────────────────────────────────────────────────────────────

    /**
     * Uploads each [PendingAttachment] first, then creates the memo with the
     * returned attachment names.
     */
    suspend fun createMemo(
        content: String,
        visibility: MemoVisibility,
        attachments: List<PendingAttachment>,
    ): MemosMemo? = withContext(Dispatchers.IO) {
        val attachmentNames = attachments.mapNotNull { pending ->
            client.createAttachment(
                filename = pending.fileName,
                mimeType = pending.mimeType,
                bytes = pending.bytes,
            )?.name
        }
        val memo = client.createMemo(
            content = content,
            visibility = visibility,
            attachmentNames = attachmentNames,
        ) ?: return@withContext null
        runCatching { dao.upsert(memo.toEntity()) }
            .onFailure { e -> Log.e(tag, "createMemo DB insert failed", e) }
        memo
    }

    // ─── Delete ───────────────────────────────────────────────────────────────

    suspend fun deleteMemo(name: String, force: Boolean = false) = withContext(Dispatchers.IO) {
        client.deleteMemo(name, force)
        runCatching { dao.deleteByName(name) }
            .onFailure { e -> Log.e(tag, "deleteMemo DB remove failed", e) }
    }

    // ─── Archive ──────────────────────────────────────────────────────────────

    suspend fun archiveMemo(name: String): MemosMemo? = withContext(Dispatchers.IO) {
        val updated = client.updateMemo(
            memoId = name,
            state = MemoState.ARCHIVED,
            updateMask = "state",
        ) ?: return@withContext null
        runCatching { dao.upsert(updated.toEntity()) }
            .onFailure { e -> Log.e(tag, "archiveMemo DB update failed", e) }
        updated
    }

    suspend fun restoreMemo(name: String): MemosMemo? = withContext(Dispatchers.IO) {
        val updated = client.updateMemo(
            memoId = name,
            state = MemoState.NORMAL,
            updateMask = "state",
        ) ?: return@withContext null
        runCatching { dao.upsert(updated.toEntity()) }
            .onFailure { e -> Log.e(tag, "restoreMemo DB update failed", e) }
        updated
    }

    // ─── Comments ─────────────────────────────────────────────────────────────

    /** Live network call — comments are not cached locally. */
    suspend fun loadComments(memoId: String): List<MemosMemo> = withContext(Dispatchers.IO) {
        client.listMemoComments(memoId)
    }

    suspend fun createComment(parentId: String, content: String): MemosMemo? =
        withContext(Dispatchers.IO) {
            client.createMemoComment(parentId, content)
        }

    // ─── Share ────────────────────────────────────────────────────────────────

    /** Returns the full share URL or null on error. */
    suspend fun createShare(memoId: String): String? = withContext(Dispatchers.IO) {
        client.createMemoShare(memoId)
    }

    // ─── Share resolution ─────────────────────────────────────────────────────

    suspend fun getMemoByShare(shareId: String): MemosMemo? = withContext(Dispatchers.IO) {
        client.getMemoByShare(shareId)
    }
}

/** Pending file to attach — mirrors GiteaClient PendingAttachment pattern from IssuesViewModel. */
data class PendingAttachment(
    val fileName: String,
    val mimeType: String,
    val bytes: ByteArray,
) {
    override fun equals(other: Any?): Boolean =
        other is PendingAttachment && fileName == other.fileName && mimeType == other.mimeType

    override fun hashCode(): Int = 31 * fileName.hashCode() + mimeType.hashCode()
}
