package com.crossdashboard.app.data.network

import android.accounts.AccountManager
import android.content.Context
import android.content.Intent
import com.nextcloud.android.sso.AccountImporter
import com.nextcloud.android.sso.exceptions.NextcloudFilesAppAccountNotFoundException
import com.nextcloud.android.sso.helper.SingleAccountHelper
import com.nextcloud.android.sso.model.SingleSignOnAccount
import dagger.hilt.android.qualifiers.ApplicationContext
import javax.inject.Inject
import javax.inject.Singleton

/**
 * Wrapper for the Nextcloud Android-SingleSignOn library.
 *
 * Usage in a Composable:
 *  1. Check `isAvailable(context)` to see if the Nextcloud Files app is installed.
 *  2. Launch `AccountImporter.pickNewAccount(activity)` from a button click.
 *  3. In onActivityResult (or ActivityResultLauncher), call `handleActivityResult()`.
 *  4. On success, store the SSO account name via `saveAccount()`.
 *  5. Subsequent CalDAV requests use `getAccount()` to obtain credentials.
 *
 * NOTE: When using Nextcloud SSO, CalDAV requests MUST be routed through
 * NextcloudAPI.performNetworkRequest() rather than plain OkHttp, because the
 * token is only valid when proxied through the Nextcloud Files app AIDL service.
 * See NextcloudSsoCalDavClient for the SSO-aware CalDAV implementation.
 */
@Singleton
class NextcloudSsoHelper @Inject constructor(
    @ApplicationContext private val context: Context,
) {
    fun isNextcloudAppInstalled(): Boolean {
        return try {
            context.packageManager.getPackageInfo("com.nextcloud.client", 0) != null
        } catch (_: Exception) {
            try {
                context.packageManager.getPackageInfo("com.nextcloud.android", 0) != null
            } catch (_: Exception) { false }
        }
    }

    fun getStoredAccount(): SingleSignOnAccount? {
        return try {
            SingleAccountHelper.getCurrentSingleSignOnAccount(context)
        } catch (_: Exception) { null }
    }

    fun commitAccount(accountName: String) {
        SingleAccountHelper.commitCurrentAccount(context, accountName)
    }

    fun clearAccount() {
        SingleAccountHelper.commitCurrentAccount(context, null)
    }

    /**
     * Returns the Intent for the standard Android account picker filtered to Nextcloud accounts.
     * This replicates what AccountImporter.pickNewAccount(activity) builds internally but as a
     * plain Intent so it can be launched via ActivityResultLauncher.
     *
     * Step 1 of the two-step SSO flow; the caller must pass the result back to
     * AccountImporter.onActivityResult(CHOOSE_ACCOUNT_SSO, ...) which internally fires
     * step 2 (REQUEST_AUTH_TOKEN_SSO) via Activity.startActivityForResult.
     */
    fun buildPickAccountIntent(context: Context): Intent? {
        return try {
            // Retrieve the registered Nextcloud account types from the SSO library's internal
            // registry (e.g. ["nextcloud", "nextcloud.owncloud"]), falling back to the
            // well-known default if reflection fails on a future library version.
            val accountTypes = runCatching {
                @Suppress("UNCHECKED_CAST")
                Class.forName("com.nextcloud.android.sso.FilesAppTypeRegistry")
                    .getMethod("getInstance").invoke(null)
                    .let { it.javaClass.getMethod("getAccountTypes").invoke(it) as Array<String> }
            }.getOrElse { arrayOf("nextcloud") }

            AccountManager.newChooseAccountIntent(
                null, null, accountTypes,
                true, null, "SSO", null, null,
            )
        } catch (_: Exception) { null }
    }

    companion object {
        /** Request code for AccountImporter.pickNewAccount() */
        const val REQUEST_CODE_SSO = 4321
    }
}
