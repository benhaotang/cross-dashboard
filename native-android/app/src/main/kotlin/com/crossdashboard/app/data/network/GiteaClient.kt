package com.crossdashboard.app.data.network

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
class GiteaClient @Inject constructor(
    private val secureStore: SecureStore,
    private val httpClient: OkHttpClient,
) {
    private val json = Json { ignoreUnknownKeys = true; coerceInputValues = true }

    // ─── Issues ───────────────────────────────────────────────────────────────

    suspend fun fetchIssues(repositories: List<String>, state: String = "open"): List<GiteaIssue> =
        withContext(Dispatchers.IO) {
            val base = instanceUrl() ?: return@withContext emptyList()
            val results = mutableListOf<GiteaIssue>()
            for (repo in repositories) {
                var page = 1
                while (true) {
                    val url = "$base/api/v1/repos/$repo/issues?state=$state&type=issues&limit=50&page=$page"
                    val response = get(url) ?: break
                    val dto = runCatching { json.decodeFromString<List<GiteaIssueDto>>(response) }
                        .getOrNull() ?: break
                    if (dto.isEmpty()) break
                    results.addAll(dto.map { it.toDomain(repo) })
                    page++
                    if (dto.size < 50) break
                }
            }
            results
        }

    suspend fun updateIssue(
        repo: String,
        number: Int,
        title: String? = null,
        body: String? = null,
        state: String? = null,
    ): GiteaIssue = withContext(Dispatchers.IO) {
        val base = instanceUrl() ?: throw IOException("No Gitea instance configured")
        val payload = buildString {
            append("{")
            val parts = mutableListOf<String>()
            title?.let { parts.add(""""title":${json.encodeToString(it)}""") }
            body?.let { parts.add(""""body":${json.encodeToString(it)}""") }
            state?.let { parts.add(""""state":"$it"""") }
            append(parts.joinToString(","))
            append("}")
        }
        val response = patch("$base/api/v1/repos/$repo/issues/$number", payload)
            ?: throw IOException("Update failed")
        json.decodeFromString<GiteaIssueDto>(response).toDomain(repo)
    }

    suspend fun fetchComments(repo: String, number: Int): List<GiteaComment> =
        withContext(Dispatchers.IO) {
            val base = instanceUrl() ?: return@withContext emptyList()
            val response = get("$base/api/v1/repos/$repo/issues/$number/comments") ?: return@withContext emptyList()
            runCatching { json.decodeFromString<List<GiteaCommentDto>>(response) }
                .getOrNull()
                ?.map { it.toDomain() }
                ?: emptyList()
        }

    suspend fun addComment(repo: String, number: Int, body: String): GiteaComment =
        withContext(Dispatchers.IO) {
            val base = instanceUrl() ?: throw IOException("No Gitea instance configured")
            val payload = """{"body":${json.encodeToString(body)}}"""
            val response = post("$base/api/v1/repos/$repo/issues/$number/comments", payload)
                ?: throw IOException("Comment failed")
            json.decodeFromString<GiteaCommentDto>(response).toDomain()
        }

    suspend fun fetchLabels(repo: String): List<GiteaLabel> = withContext(Dispatchers.IO) {
        val base = instanceUrl() ?: return@withContext emptyList()
        val response = get("$base/api/v1/repos/$repo/labels") ?: return@withContext emptyList()
        runCatching { json.decodeFromString<List<GiteaLabelDto>>(response) }
            .getOrNull()?.map { GiteaLabel(it.id, it.name, it.color) }
            ?: emptyList()
    }

    suspend fun createRepoLabel(repo: String, name: String, color: String = "0075ca"): GiteaLabel =
        withContext(Dispatchers.IO) {
            val base = instanceUrl() ?: throw IOException("No instance configured")
            val payload = """{"name":"$name","color":"#$color"}"""
            val response = post("$base/api/v1/repos/$repo/labels", payload)
                ?: throw IOException("Label creation failed")
            val dto = json.decodeFromString<GiteaLabelDto>(response)
            GiteaLabel(dto.id, dto.name, dto.color)
        }

    suspend fun replaceIssueLabels(repo: String, number: Int, labelIds: List<Long>): Unit =
        withContext(Dispatchers.IO) {
            val base = instanceUrl() ?: throw IOException("No instance configured")
            val payload = """{"labels":[${labelIds.joinToString(",")}]}"""
            put("$base/api/v1/repos/$repo/issues/$number/labels", payload)
        }

    suspend fun createIssue(repo: String, title: String, body: String): GiteaIssue =
        withContext(Dispatchers.IO) {
            val base = instanceUrl() ?: throw IOException("No Gitea instance configured")
            val payload = buildString {
                append("{")
                append(""""title":${json.encodeToString(title)}""")
                if (body.isNotBlank()) append(""","body":${json.encodeToString(body)}""")
                append("}")
            }
            val response = post("$base/api/v1/repos/$repo/issues", payload)
                ?: throw IOException("Create issue failed")
            json.decodeFromString<GiteaIssueDto>(response).toDomain(repo)
        }

    suspend fun uploadIssueAttachment(
        repo: String,
        issueNumber: Int,
        fileName: String,
        bytes: ByteArray,
        mimeType: String,
    ): String = withContext(Dispatchers.IO) {
        val base = instanceUrl() ?: throw IOException("No Gitea instance configured")
        val body = MultipartBody.Builder()
            .setType(MultipartBody.FORM)
            .addFormDataPart("attachment", fileName, bytes.toRequestBody(mimeType.toMediaType()))
            .build()
        val request = Request.Builder()
            .url("$base/api/v1/repos/$repo/issues/$issueNumber/assets")
            .addToken()
            .post(body)
            .build()
        httpClient.newCall(request).execute().use { r ->
            if (!r.isSuccessful) throw IOException("Attachment upload failed: ${r.code}")
            val dto = json.decodeFromString<GiteaAttachmentDto>(
                r.body?.string() ?: throw IOException("Empty response")
            )
            dto.browser_download_url
        }
    }

    suspend fun uploadCommentAttachment(
        repo: String,
        commentId: Long,
        fileName: String,
        bytes: ByteArray,
        mimeType: String,
    ): String = withContext(Dispatchers.IO) {
        val base = instanceUrl() ?: throw IOException("No Gitea instance configured")
        val body = MultipartBody.Builder()
            .setType(MultipartBody.FORM)
            .addFormDataPart("attachment", fileName, bytes.toRequestBody(mimeType.toMediaType()))
            .build()
        val request = Request.Builder()
            .url("$base/api/v1/repos/$repo/issues/comments/$commentId/assets")
            .addToken()
            .post(body)
            .build()
        httpClient.newCall(request).execute().use { r ->
            if (!r.isSuccessful) throw IOException("Attachment upload failed: ${r.code}")
            val dto = json.decodeFromString<GiteaAttachmentDto>(
                r.body?.string() ?: throw IOException("Empty response")
            )
            dto.browser_download_url
        }
    }

    // ─── HTTP helpers ─────────────────────────────────────────────────────────

    private fun get(url: String): String? {
        return try {
            httpClient.newCall(Request.Builder().url(url).addToken().build()).execute().use { r ->
                if (r.isSuccessful) r.body?.string() else null
            }
        } catch (_: Exception) { null }
    }

    private fun post(url: String, json: String): String? {
        val body = json.toRequestBody("application/json".toMediaType())
        return try {
            httpClient.newCall(Request.Builder().url(url).addToken().post(body).build()).execute().use { r ->
                if (r.isSuccessful) r.body?.string() else null
            }
        } catch (_: Exception) { null }
    }

    private fun patch(url: String, json: String): String? {
        val body = json.toRequestBody("application/json".toMediaType())
        return try {
            httpClient.newCall(Request.Builder().url(url).addToken().patch(body).build()).execute().use { r ->
                if (r.isSuccessful) r.body?.string() else null
            }
        } catch (_: Exception) { null }
    }

    private fun put(url: String, json: String): String? {
        val body = json.toRequestBody("application/json".toMediaType())
        return try {
            httpClient.newCall(Request.Builder().url(url).addToken().put(body).build()).execute().use { r ->
                if (r.isSuccessful) r.body?.string() else null
            }
        } catch (_: Exception) { null }
    }

    private fun Request.Builder.addToken(): Request.Builder {
        val token = secureStore.get(CredentialKey.GITEA_TOKEN) ?: return this
        return header("Authorization", "token $token")
    }

    private fun instanceUrl(): String? =
        secureStore.get(CredentialKey.GITEA_INSTANCE)?.trimEnd('/')

    // ─── DTOs ─────────────────────────────────────────────────────────────────

    @Serializable
    private data class GiteaIssueDto(
        val id: Long,
        val number: Int,
        val title: String,
        val body: String = "",
        val state: String,
        val labels: List<GiteaLabelDto> = emptyList(),
        val assignees: List<GiteaUserDto> = emptyList(),
        val created_at: String,
        val updated_at: String,
        val html_url: String,
    ) {
        fun toDomain(repo: String) = GiteaIssue(
            id = id,
            number = number,
            title = title,
            body = body,
            state = state,
            labels = labels.map { it.name },
            assignees = assignees.map { it.login },
            createdAt = Instant.parse(created_at),
            updatedAt = Instant.parse(updated_at),
            repository = repo,
            htmlUrl = html_url,
        )
    }

    @Serializable
    private data class GiteaCommentDto(
        val id: Long,
        val body: String,
        val user: GiteaUserDto,
        val created_at: String,
    ) {
        fun toDomain() = GiteaComment(
            id = id,
            body = body,
            user = user.login,
            createdAt = Instant.parse(created_at),
        )
    }

    @Serializable
    private data class GiteaLabelDto(val id: Long, val name: String, val color: String)

    @Serializable
    private data class GiteaUserDto(val login: String)

    @Serializable
    private data class GiteaAttachmentDto(
        val id: Long = 0,
        val name: String = "",
        val browser_download_url: String = "",
        val size: Long = 0,
        val uuid: String = "",
    )
}
