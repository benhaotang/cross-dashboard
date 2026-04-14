package com.crossdashboard.app.data.network

import android.util.Base64
import com.crossdashboard.app.data.prefs.CredentialKey
import com.crossdashboard.app.data.prefs.SecureStore
import com.crossdashboard.app.domain.model.*
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import kotlinx.serialization.Serializable
import kotlinx.serialization.json.Json
import okhttp3.*
import okhttp3.MediaType.Companion.toMediaType
import okhttp3.RequestBody.Companion.toRequestBody
import java.io.IOException
import java.time.Instant
import javax.inject.Inject
import javax.inject.Singleton

@Singleton
class MemosClient @Inject constructor(
    private val secureStore: SecureStore,
    private val httpClient: OkHttpClient,
) {
    private val json = Json { ignoreUnknownKeys = true; coerceInputValues = true }

    // ─── Memos list ───────────────────────────────────────────────────────────

    /** Returns a page of memos. Pass the returned nextPageToken to fetch the next page. */
    suspend fun listMemos(
        pageToken: String? = null,
        filter: String? = null,
        state: MemoState = MemoState.NORMAL,
        orderBy: String = "display_time desc",
        pageSize: Int = 50,
    ): Pair<List<MemosMemo>, String?> = withContext(Dispatchers.IO) {
        val base = baseUrl() ?: return@withContext Pair(emptyList(), null)
        val sb = StringBuilder("$base/api/v1/memos?pageSize=$pageSize&orderBy=${orderBy.encodeUrl()}")
        if (state != MemoState.NORMAL) sb.append("&state=${state.name}")
        filter?.let { sb.append("&filter=${it.encodeUrl()}") }
        pageToken?.let { sb.append("&pageToken=${it.encodeUrl()}") }
        val response = get(sb.toString()) ?: return@withContext Pair(emptyList(), null)
        val dto = runCatching { json.decodeFromString<MemoListDto>(response) }.getOrNull()
            ?: return@withContext Pair(emptyList(), null)
        val memos = dto.memos.mapNotNull { it.toDomain() }
        Pair(memos, dto.nextPageToken?.takeIf { it.isNotBlank() })
    }

    suspend fun getMemo(memoId: String): MemosMemo? = withContext(Dispatchers.IO) {
        val base = baseUrl() ?: return@withContext null
        val response = get("$base/api/v1/$memoId") ?: return@withContext null
        runCatching { json.decodeFromString<MemoDtoFull>(response) }.getOrNull()?.toDomain()
    }

    // ─── Create / Update / Delete ─────────────────────────────────────────────

    suspend fun createMemo(
        content: String,
        visibility: MemoVisibility = MemoVisibility.PRIVATE,
        attachmentNames: List<String> = emptyList(),
    ): MemosMemo? = withContext(Dispatchers.IO) {
        val base = baseUrl() ?: return@withContext null
        val attachmentsJson = if (attachmentNames.isEmpty()) ""
        else ""","attachments":${json.encodeToString(attachmentNames.map { mapOf("name" to it) })}"""
        val payload = """{"state":"NORMAL","content":${json.encodeToString(content)},"visibility":"${visibility.name}"$attachmentsJson}"""
        val response = post("$base/api/v1/memos", payload) ?: return@withContext null
        runCatching { json.decodeFromString<MemoDtoFull>(response) }.getOrNull()?.toDomain()
    }

    suspend fun updateMemo(
        memoId: String,
        content: String? = null,
        state: MemoState? = null,
        updateMask: String,
    ): MemosMemo? = withContext(Dispatchers.IO) {
        val base = baseUrl() ?: return@withContext null
        val parts = mutableListOf<String>()
        content?.let { parts.add(""""content":${json.encodeToString(it)}""") }
        state?.let { parts.add(""""state":"${it.name}"""") }
        if (parts.isEmpty()) return@withContext null
        val payload = "{${parts.joinToString(",")},\"state\":\"${state?.name ?: "NORMAL"}\"}"
        val response = patch(
            "$base/api/v1/$memoId?updateMask=${updateMask.encodeUrl()}",
            payload,
        ) ?: return@withContext null
        runCatching { json.decodeFromString<MemoDtoFull>(response) }.getOrNull()?.toDomain()
    }

    suspend fun deleteMemo(memoId: String, force: Boolean = false): Boolean =
        withContext(Dispatchers.IO) {
            val base = baseUrl() ?: return@withContext false
            val url = "$base/api/v1/$memoId" + if (force) "?force=true" else ""
            delete(url)
        }

    // ─── Comments ─────────────────────────────────────────────────────────────

    suspend fun listMemoComments(memoId: String, pageToken: String? = null): List<MemosMemo> =
        withContext(Dispatchers.IO) {
            val base = baseUrl() ?: return@withContext emptyList()
            var url = "$base/api/v1/$memoId/comments"
            pageToken?.let { url += "?pageToken=${it.encodeUrl()}" }
            val response = get(url) ?: return@withContext emptyList()
            runCatching { json.decodeFromString<MemoListDto>(response) }.getOrNull()
                ?.memos?.mapNotNull { it.toDomain() }
                ?: emptyList()
        }

    suspend fun createMemoComment(
        parentMemoId: String,
        content: String,
        visibility: MemoVisibility = MemoVisibility.PRIVATE,
    ): MemosMemo? = withContext(Dispatchers.IO) {
        val base = baseUrl() ?: return@withContext null
        val payload = """{"state":"NORMAL","content":${json.encodeToString(content)},"visibility":"${visibility.name}"}"""
        val response = post("$base/api/v1/$parentMemoId/comments", payload)
            ?: return@withContext null
        runCatching { json.decodeFromString<MemoDtoFull>(response) }.getOrNull()?.toDomain()
    }

    // ─── Attachments ──────────────────────────────────────────────────────────

    suspend fun listMemoAttachments(memoId: String): List<MemosAttachment> =
        withContext(Dispatchers.IO) {
            val base = baseUrl() ?: return@withContext emptyList()
            val response = get("$base/api/v1/$memoId/attachments") ?: return@withContext emptyList()
            runCatching { json.decodeFromString<AttachmentListDto>(response) }.getOrNull()
                ?.attachments?.map { it.toDomain() }
                ?: emptyList()
        }

    /**
     * Uploads a file as a Memos attachment.
     * Content must be Base64-encoded; the API accepts it in the JSON body — no multipart needed.
     */
    suspend fun createAttachment(
        filename: String,
        mimeType: String,
        bytes: ByteArray,
        memoName: String? = null,
    ): MemosAttachment? = withContext(Dispatchers.IO) {
        val base = baseUrl() ?: return@withContext null
        val b64 = Base64.encodeToString(bytes, Base64.NO_WRAP)
        val memoPart = memoName?.let { ""","memo":"$it"""" } ?: ""
        val payload = """{"filename":${json.encodeToString(filename)},"type":"$mimeType","content":"$b64"$memoPart}"""
        val response = post("$base/api/v1/attachments", payload) ?: return@withContext null
        runCatching { json.decodeFromString<AttachmentDto>(response) }.getOrNull()?.toDomain()
    }

    suspend fun deleteAttachments(names: List<String>): Boolean = withContext(Dispatchers.IO) {
        val base = baseUrl() ?: return@withContext false
        val payload = """{"names":${json.encodeToString(names)}}"""
        post("$base/api/v1/attachments:batchDelete", payload) != null
    }

    // ─── Relations ────────────────────────────────────────────────────────────

    suspend fun listMemoRelations(memoId: String): List<MemoRelation> =
        withContext(Dispatchers.IO) {
            val base = baseUrl() ?: return@withContext emptyList()
            val response = get("$base/api/v1/$memoId/relations") ?: return@withContext emptyList()
            runCatching { json.decodeFromString<RelationListDto>(response) }.getOrNull()
                ?.relations?.map {
                    MemoRelation(
                        memoName = it.memo?.name ?: "",
                        memoSnippet = it.memo?.snippet ?: "",
                        relatedMemoName = it.relatedMemo?.name ?: "",
                        relatedMemoSnippet = it.relatedMemo?.snippet ?: "",
                    )
                }
                ?: emptyList()
        }

    // ─── Shares ───────────────────────────────────────────────────────────────

    /** Creates a share link and returns the full share URL: {host}/s/{token} */
    suspend fun createMemoShare(memoId: String, expireTimeIso: String? = null): String? =
        withContext(Dispatchers.IO) {
            val base = baseUrl() ?: return@withContext null
            val payload = if (expireTimeIso != null)
                """{"expireTime":"$expireTimeIso"}"""
            else "{}"
            val response = post("$base/api/v1/$memoId/shares", payload) ?: return@withContext null
            val dto = runCatching { json.decodeFromString<ShareDto>(response) }.getOrNull()
                ?: return@withContext null
            // name format: "memos/{id}/shares/{token}" — extract the last segment
            val token = dto.name.substringAfterLast("/")
            "$base/s/$token"
        }

    // ─── Share resolution (no auth) ───────────────────────────────────────────

    suspend fun getMemoByShare(shareId: String): MemosMemo? = withContext(Dispatchers.IO) {
        val base = baseUrl() ?: return@withContext null
        // Share resolution requires no auth — use a plain request without the token header
        val request = Request.Builder().url("$base/api/v1/shares/$shareId").build()
        val response = try {
            httpClient.newCall(request).execute().use { r ->
                if (r.isSuccessful) r.body?.string() else null
            }
        } catch (_: Exception) { null } ?: return@withContext null
        runCatching { json.decodeFromString<MemoDtoFull>(response) }.getOrNull()?.toDomain()
    }

    // ─── HTTP helpers ─────────────────────────────────────────────────────────

    private fun get(url: String): String? = try {
        httpClient.newCall(Request.Builder().url(url).addToken().build()).execute().use { r ->
            if (r.isSuccessful) r.body?.string() else null
        }
    } catch (_: Exception) { null }

    private fun post(url: String, jsonBody: String): String? {
        val body = jsonBody.toRequestBody("application/json".toMediaType())
        return try {
            httpClient.newCall(Request.Builder().url(url).addToken().post(body).build())
                .execute().use { r -> if (r.isSuccessful) r.body?.string() else null }
        } catch (_: Exception) { null }
    }

    private fun patch(url: String, jsonBody: String): String? {
        val body = jsonBody.toRequestBody("application/json".toMediaType())
        return try {
            httpClient.newCall(Request.Builder().url(url).addToken().patch(body).build())
                .execute().use { r -> if (r.isSuccessful) r.body?.string() else null }
        } catch (_: Exception) { null }
    }

    private fun delete(url: String): Boolean = try {
        httpClient.newCall(Request.Builder().url(url).addToken().delete().build())
            .execute().use { r -> r.isSuccessful }
    } catch (_: Exception) { false }

    private fun Request.Builder.addToken(): Request.Builder {
        val token = secureStore.get(CredentialKey.MEMOS_TOKEN) ?: return this
        return header("Authorization", "Bearer $token")
    }

    fun baseUrl(): String? =
        secureStore.get(CredentialKey.MEMOS_HOST)?.trimEnd('/')

    private fun String.encodeUrl(): String =
        java.net.URLEncoder.encode(this, "UTF-8")

    // ─── DTOs ─────────────────────────────────────────────────────────────────

    @Serializable
    private data class MemoListDto(
        val memos: List<MemoDtoFull> = emptyList(),
        val nextPageToken: String? = null,
    )

    @Serializable
    private data class MemoDtoFull(
        val name: String = "",
        val state: String = "STATE_UNSPECIFIED",
        val content: String = "",
        val visibility: String = "VISIBILITY_UNSPECIFIED",
        val tags: List<String> = emptyList(),
        val pinned: Boolean = false,
        val attachments: List<AttachmentDto> = emptyList(),
        val property: MemoPropertyDto = MemoPropertyDto(),
        val snippet: String = "",
        val createTime: String = "",
        val displayTime: String = "",
        val updateTime: String = "",
    ) {
        fun toDomain(): MemosMemo? {
            val create = runCatching { Instant.parse(createTime) }.getOrElse { Instant.now() }
            val display = runCatching { Instant.parse(displayTime) }.getOrElse { Instant.now() }
            val update = runCatching { Instant.parse(updateTime) }.getOrElse { Instant.now() }
            return MemosMemo(
                name = name,
                state = when (state) { "ARCHIVED" -> MemoState.ARCHIVED; else -> MemoState.NORMAL },
                content = content,
                visibility = when (visibility) {
                    "PUBLIC" -> MemoVisibility.PUBLIC
                    "PROTECTED" -> MemoVisibility.PROTECTED
                    else -> MemoVisibility.PRIVATE
                },
                tags = tags,
                pinned = pinned,
                attachments = attachments.map { it.toDomain() },
                property = property.toDomain(),
                snippet = snippet,
                createTime = create,
                displayTime = display,
                updateTime = update,
            )
        }
    }

    @Serializable
    data class AttachmentDto(
        val name: String = "",
        val filename: String = "",
        val externalLink: String = "",
        val type: String = "",
        val size: String = "0",
        val memo: String = "",
    ) {
        fun toDomain() = MemosAttachment(
            name = name,
            filename = filename,
            externalLink = externalLink,
            type = type,
            size = size.toLongOrNull() ?: 0L,
            memo = memo,
        )
    }

    @Serializable
    private data class AttachmentListDto(val attachments: List<AttachmentDto> = emptyList())

    @Serializable
    private data class MemoPropertyDto(
        val hasLink: Boolean = false,
        val hasTaskList: Boolean = false,
        val hasIncompleteTasks: Boolean = false,
        val title: String = "",
    ) {
        fun toDomain() = MemoProperty(hasLink, hasTaskList, hasIncompleteTasks, title)
    }

    @Serializable
    private data class RelationListDto(val relations: List<RelationDto> = emptyList())

    @Serializable
    private data class RelationDto(
        val memo: RelationMemoDto? = null,
        val relatedMemo: RelationMemoDto? = null,
        val type: String = "",
    )

    @Serializable
    private data class RelationMemoDto(
        val name: String = "",
        val snippet: String = "",
    )

    @Serializable
    private data class ShareDto(val name: String = "")
}

/** Lightweight relation summary returned from listMemoRelations. */
data class MemoRelation(
    val memoName: String,
    val memoSnippet: String,
    val relatedMemoName: String,
    val relatedMemoSnippet: String,
)
