package com.crossdashboard.app.data.network

import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.flow.SharedFlow
import kotlinx.coroutines.flow.asSharedFlow
import javax.inject.Inject
import javax.inject.Singleton

/**
 * Event bus for the second step of the Nextcloud SSO flow.
 *
 * Because AccountImporter's two-step auth flow requires a second startActivityForResult
 * (REQUEST_AUTH_TOKEN_SSO = 4243) that is launched from within onActivityResult — bypassing
 * any ActivityResultLauncher — its result arrives in MainActivity.onActivityResult.
 * This bus lets MainActivity forward that result to SettingsViewModel.
 *
 * null = sign-in cancelled or failed.
 */
@Singleton
class SsoResultBus @Inject constructor() {
    private val _accountName = MutableSharedFlow<String?>(extraBufferCapacity = 1)
    val accountName: SharedFlow<String?> = _accountName.asSharedFlow()

    fun post(name: String?) {
        _accountName.tryEmit(name)
    }
}
