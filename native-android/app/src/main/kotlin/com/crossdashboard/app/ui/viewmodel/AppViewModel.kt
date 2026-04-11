package com.crossdashboard.app.ui.viewmodel

import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.crossdashboard.app.data.prefs.AppPreferences
import com.crossdashboard.app.domain.model.ThemePreference
import dagger.hilt.android.lifecycle.HiltViewModel
import kotlinx.coroutines.flow.*
import kotlinx.coroutines.launch
import javax.inject.Inject

@HiltViewModel
class AppViewModel @Inject constructor(
    private val prefs: AppPreferences,
) : ViewModel() {

    val theme: StateFlow<ThemePreference> = prefs.themeFlow
        .stateIn(viewModelScope, SharingStarted.Eagerly, ThemePreference.SYSTEM)

    val visibleScreens: StateFlow<List<String>> = prefs.visibleScreensFlow
        .stateIn(viewModelScope, SharingStarted.Eagerly, emptyList())

    val biometricLock: StateFlow<Boolean> = prefs.biometricLockFlow
        .stateIn(viewModelScope, SharingStarted.Eagerly, false)

    /** True when the user has enabled biometric / device-credential as a convenience unlock. */
    val systemCredential: StateFlow<Boolean> = prefs.systemCredentialFlow
        .stateIn(viewModelScope, SharingStarted.Eagerly, false)

    // Unlock state — backed by Compose mutableStateOf so CrossDashboardRoot recomposes
    // immediately when the user successfully authenticates.
    var isUnlocked by mutableStateOf(false)
        private set

    fun unlock() { isUnlocked = true }

    fun setTheme(theme: ThemePreference) {
        viewModelScope.launch { prefs.setTheme(theme) }
    }

    fun setVisibleScreens(screens: List<String>) {
        viewModelScope.launch { prefs.setVisibleScreens(screens) }
    }
}
