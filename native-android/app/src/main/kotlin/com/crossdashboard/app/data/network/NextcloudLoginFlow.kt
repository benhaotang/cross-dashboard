package com.crossdashboard.app.data.network

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.withContext
import kotlinx.coroutines.withTimeout
import kotlinx.serialization.Serializable
import kotlinx.serialization.json.Json
import okhttp3.*
import okhttp3.MediaType.Companion.toMediaType
import okhttp3.RequestBody.Companion.toRequestBody
import javax.inject.Inject
import javax.inject.Singleton

/**
 * Nextcloud Login Flow v2 (browser-based).
 *
 * Flow:
 * 1. POST to /index.php/login/v2 → get loginUrl + pollToken + pollEndpoint
 * 2. Open loginUrl in a Custom Tab — user approves in browser
 * 3. Poll pollEndpoint with pollToken until credentials arrive (max 5 min)
 * 4. Returns (serverUrl, loginName, appPassword) to store in SecureStore
 */
@Singleton
class NextcloudLoginFlow @Inject constructor(
    private val httpClient: OkHttpClient,
) {
    private val json = Json { ignoreUnknownKeys = true }

    data class FlowInit(
        val loginUrl: String,
        val pollEndpoint: String,
        val pollToken: String,
    )

    data class LoginCredentials(
        val serverUrl: String,
        val loginName: String,
        val appPassword: String,
    )

    suspend fun initiate(serverUrl: String): Result<FlowInit> = withContext(Dispatchers.IO) {
        try {
            val url = "${serverUrl.trimEnd('/')}/index.php/login/v2"
            val body = "".toRequestBody("application/x-www-form-urlencoded".toMediaType())
            val response = httpClient.newCall(
                Request.Builder().url(url).post(body).build()
            ).execute()
            if (!response.isSuccessful) return@withContext Result.failure(Exception("HTTP ${response.code}"))
            val dto = json.decodeFromString<LoginFlowInitDto>(response.body!!.string())
            Result.success(FlowInit(dto.login, dto.poll.endpoint, dto.poll.token))
        } catch (e: Exception) {
            Result.failure(e)
        }
    }

    suspend fun poll(
        pollEndpoint: String,
        pollToken: String,
        timeoutMs: Long = 5 * 60_000L,
    ): Result<LoginCredentials> = withContext(Dispatchers.IO) {
        try {
            withTimeout(timeoutMs) {
                val body = "token=$pollToken".toRequestBody("application/x-www-form-urlencoded".toMediaType())
                while (true) {
                    delay(2000)
                    val response = httpClient.newCall(
                        Request.Builder().url(pollEndpoint).post(body).build()
                    ).execute()
                    if (response.code == 200) {
                        val dto = json.decodeFromString<LoginCredentialsDto>(response.body!!.string())
                        return@withTimeout Result.success(
                            LoginCredentials(dto.server, dto.loginName, dto.appPassword)
                        )
                    }
                    // 404 = not yet approved; keep polling
                }
                @Suppress("UNREACHABLE_CODE")
                Result.failure(Exception("Timeout"))
            }
        } catch (e: Exception) {
            Result.failure(e)
        }
    }

    @Serializable
    private data class LoginFlowInitDto(val poll: PollDto, val login: String)

    @Serializable
    private data class PollDto(val token: String, val endpoint: String)

    @Serializable
    private data class LoginCredentialsDto(
        val server: String,
        val loginName: String,
        val appPassword: String,
    )
}
