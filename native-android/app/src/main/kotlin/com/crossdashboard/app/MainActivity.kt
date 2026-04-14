package com.crossdashboard.app

import android.content.Intent
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.os.Parcelable
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.core.splashscreen.SplashScreen.Companion.installSplashScreen
import androidx.lifecycle.lifecycleScope
import com.crossdashboard.app.data.network.SsoResultBus
import com.crossdashboard.app.data.prefs.AppPreferences
import com.crossdashboard.app.data.repository.PendingAttachment
import com.crossdashboard.app.ui.CrossDashboardRoot
import com.nextcloud.android.sso.AccountImporter
import dagger.hilt.android.AndroidEntryPoint
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import javax.inject.Inject

@AndroidEntryPoint
class MainActivity : ComponentActivity() {

    @Inject lateinit var prefs: AppPreferences
    @Inject lateinit var ssoResultBus: SsoResultBus

    // Deep link action forwarded to nav graph
    private var pendingAction by mutableStateOf<String?>(null)
    // Share payload shown as ShareCaptureSheet overlay
    var pendingSharePayload by mutableStateOf<SharePayload?>(null)

    override fun onCreate(savedInstanceState: Bundle?) {
        val splashScreen = installSplashScreen()

        super.onCreate(savedInstanceState)
        enableEdgeToEdge()

        pendingAction = intent.data?.resolvePendingAction()
        lifecycleScope.launch { pendingSharePayload = parseShareIntent(intent) }

        setContent {
            // Theme is applied inside CrossDashboardRoot based on user preference.
            CrossDashboardRoot(
                pendingAction = pendingAction,
                onActionConsumed = { pendingAction = null },
                pendingSharePayload = pendingSharePayload,
                onShareConsumed = { pendingSharePayload = null },
            )
        }
    }

    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        setIntent(intent)
        pendingAction = intent.data?.resolvePendingAction()
        lifecycleScope.launch { pendingSharePayload = parseShareIntent(intent) }
    }

    private suspend fun parseShareIntent(intent: Intent): SharePayload? = withContext(Dispatchers.IO) {
        val action = intent.action ?: return@withContext null
        if (action != Intent.ACTION_SEND && action != Intent.ACTION_SEND_MULTIPLE) return@withContext null

        val text = intent.getStringExtra(Intent.EXTRA_TEXT)
        val subject = intent.getStringExtra(Intent.EXTRA_SUBJECT)

        val uris = when (action) {
            Intent.ACTION_SEND -> {
                val uri = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                    intent.getParcelableExtra(Intent.EXTRA_STREAM, Uri::class.java)
                } else {
                    @Suppress("DEPRECATION") intent.getParcelableExtra<Uri>(Intent.EXTRA_STREAM)
                }
                listOfNotNull(uri)
            }
            Intent.ACTION_SEND_MULTIPLE -> {
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                    intent.getParcelableArrayListExtra(Intent.EXTRA_STREAM, Uri::class.java) ?: emptyList()
                } else {
                    @Suppress("DEPRECATION")
                    intent.getParcelableArrayListExtra<Uri>(Intent.EXTRA_STREAM) ?: emptyList()
                }
            }
            else -> emptyList()
        }

        val attachments = uris.mapNotNull { uri ->
            runCatching {
                val bytes = contentResolver.openInputStream(uri)?.readBytes() ?: return@runCatching null
                val mime = contentResolver.getType(uri) ?: "application/octet-stream"
                val name = uri.lastPathSegment ?: "file"
                PendingAttachment(name, mime, bytes)
            }.getOrNull()
        }

        if (text == null && attachments.isEmpty()) return@withContext null
        SharePayload(text = text, subject = subject, attachments = attachments)
    }

    /**
     * Handles the second step of the Nextcloud SSO flow.
     *
     * Step 1 (CHOOSE_ACCOUNT_SSO = 4242) is handled by SettingsScreen's
     * ActivityResultLauncher. Internally, AccountImporter.onActivityResult for that step
     * calls Activity.startActivityForResult(authIntent, REQUEST_AUTH_TOKEN_SSO), which
     * bypasses the launcher and arrives here as REQUEST_AUTH_TOKEN_SSO = 4243.
     */
    @Deprecated("Required for Nextcloud SSO library two-step auth flow")
    @Suppress("OVERRIDE_DEPRECATION", "DEPRECATION")
    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        @Suppress("DEPRECATION")
        super.onActivityResult(requestCode, resultCode, data)
        if (requestCode == AccountImporter.REQUEST_AUTH_TOKEN_SSO) {
            try {
                AccountImporter.onActivityResult(requestCode, resultCode, data, this) { account ->
                    ssoResultBus.post(account.name)
                }
            } catch (_: Exception) {
                ssoResultBus.post(null)
            }
        }
    }

    companion object {
        const val KEY_ACTION = "action"
        const val ACTION_ADD_TASK = "add_task"
    }
}

/** Payload parsed from an incoming share intent. */
data class SharePayload(
    val text: String?,
    val subject: String?,
    val attachments: List<PendingAttachment>,
)

/**
 * Resolves a deep-link URI to a pending action string consumed by [CrossDashboardRoot]
 * and [AppNavigation]:
 * - `crossdashboard://pomodoro`           → `"pomodoro"`     (Pomodoro notification tap)
 * - `crossdashboard://events?uid=<uid>`   → `"events:<uid>"` (event alarm notification tap)
 * - `crossdashboard://tasks?uid=<uid>`    → `"tasks:<uid>"`  (task reminder notification tap)
 * - `crossdashboard://tasks?action=add`   → `"add"`          (widget add-task shortcut)
 */
private fun Uri.resolvePendingAction(): String? = when (host) {
    "pomodoro" -> "pomodoro"
    "events" -> getQueryParameter("uid")?.let { "events:$it" }
    "tasks" -> getQueryParameter("uid")?.let { "tasks:$it" } ?: getQueryParameter("action")
    else -> getQueryParameter("action")
}
