package com.crossdashboard.app

import android.content.Intent
import android.os.Bundle
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
import com.crossdashboard.app.ui.CrossDashboardRoot
import com.nextcloud.android.sso.AccountImporter
import dagger.hilt.android.AndroidEntryPoint
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.launch
import javax.inject.Inject

@AndroidEntryPoint
class MainActivity : ComponentActivity() {

    @Inject lateinit var prefs: AppPreferences
    @Inject lateinit var ssoResultBus: SsoResultBus

    // Deep link action forwarded to nav graph
    private var pendingAction by mutableStateOf<String?>(null)

    override fun onCreate(savedInstanceState: Bundle?) {
        val splashScreen = installSplashScreen()

        super.onCreate(savedInstanceState)
        enableEdgeToEdge()

        pendingAction = intent.data?.getQueryParameter("action")

        setContent {
            // Theme is applied inside CrossDashboardRoot based on user preference.
            CrossDashboardRoot(
                pendingAction = pendingAction,
                onActionConsumed = { pendingAction = null },
            )
        }
    }

    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        pendingAction = intent.data?.getQueryParameter("action")
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
    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
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
