package com.crossdashboard.app.data.network

import com.crossdashboard.app.data.prefs.CredentialKey
import com.crossdashboard.app.data.prefs.SecureStore
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import kotlinx.serialization.Serializable
import kotlinx.serialization.encodeToString
import kotlinx.serialization.json.Json
import okhttp3.MediaType.Companion.toMediaType
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.RequestBody.Companion.toRequestBody
import java.io.IOException
import javax.inject.Inject
import javax.inject.Singleton

@Serializable
data class KarakeepFolder(
    val id: String,
    val name: String,
    val parentId: String? = null,
    val type: String = "manual",
    val userRole: String = "owner",
)

@Serializable
private data class KarakeepFolderResponse(val lists: List<KarakeepFolder> = emptyList())

@Serializable
private data class KarakeepBookmarkResponse(val id: String)

@Serializable
private data class CreateKarakeepBookmarkRequest(
    val type: String = "link",
    val url: String,
    val source: String = "api",
)

@Singleton
class KarakeepClient @Inject constructor(
    private val secureStore: SecureStore,
    private val httpClient: OkHttpClient,
) {
    private val json = Json { ignoreUnknownKeys = true }

    suspend fun listFolders(): List<KarakeepFolder> = withContext(Dispatchers.IO) {
        val response = execute(Request.Builder().url("${apiBase()}/lists").get().build())
        json.decodeFromString<KarakeepFolderResponse>(response).lists
            .filter { it.type == "manual" && it.userRole in setOf("owner", "editor") }
            .sortedBy { it.name.lowercase() }
    }

    suspend fun saveUrls(urls: List<String>, folderId: String?) = withContext(Dispatchers.IO) {
        urls.distinct().forEach { url ->
            val payload = json.encodeToString(CreateKarakeepBookmarkRequest(url = url))
            val response = execute(
                Request.Builder()
                    .url("${apiBase()}/bookmarks")
                    .post(payload.toRequestBody(JSON_MEDIA_TYPE))
                    .build(),
            )
            val bookmarkId = json.decodeFromString<KarakeepBookmarkResponse>(response).id
            if (folderId != null) {
                execute(
                    Request.Builder()
                        .url("${apiBase()}/lists/$folderId/bookmarks/$bookmarkId")
                        .put(ByteArray(0).toRequestBody(null))
                        .build(),
                    allowEmpty = true,
                )
            }
        }
    }

    private fun apiBase(): String {
        val host = secureStore.get(CredentialKey.KARAKEEP_HOST)
            ?.trim()
            ?.trimEnd('/')
            ?.takeIf { it.isNotEmpty() }
            ?: throw IOException("Karakeep server URL is missing")
        return "$host/api/v1"
    }

    private fun execute(request: Request, allowEmpty: Boolean = false): String {
        val token = secureStore.get(CredentialKey.KARAKEEP_TOKEN)
            ?.takeIf { it.isNotBlank() }
            ?: throw IOException("Karakeep API key is missing")
        val authenticated = request.newBuilder()
            .header("Accept", "application/json")
            .header("Authorization", "Bearer $token")
            .build()
        return httpClient.newCall(authenticated).execute().use { response ->
            if (!response.isSuccessful) {
                throw IOException("Karakeep returned HTTP ${response.code}")
            }
            response.body?.string().orEmpty().also {
                if (!allowEmpty && it.isEmpty()) throw IOException("Karakeep returned an empty response")
            }
        }
    }

    private companion object {
        val JSON_MEDIA_TYPE = "application/json".toMediaType()
    }
}

