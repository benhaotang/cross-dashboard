package com.crossdashboard.app.ui.screen.settings

import android.content.Context
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import androidx.work.*
import com.crossdashboard.app.data.network.CalDavClient
import com.crossdashboard.app.data.network.NextcloudLoginFlow
import com.crossdashboard.app.data.network.NextcloudSsoHelper
import com.crossdashboard.app.data.network.SsoResultBus
import com.crossdashboard.app.data.prefs.AppPreferences
import com.crossdashboard.app.data.prefs.CredentialKey
import com.crossdashboard.app.data.prefs.SecureStore
import com.crossdashboard.app.domain.model.*
import com.crossdashboard.app.ui.component.CalendarColorResolver
import com.crossdashboard.app.worker.SyncWorker
import dagger.hilt.android.lifecycle.HiltViewModel
import dagger.hilt.android.qualifiers.ApplicationContext
import kotlinx.coroutines.flow.*
import kotlinx.coroutines.launch
import kotlinx.serialization.encodeToString
import kotlinx.serialization.json.Json
import javax.inject.Inject

// ─── UI state ──────────────────────────────────────────────────────────────────

enum class CalDavConnectionStatus { IDLE, TESTING, SUCCESS, ERROR }
enum class NextcloudFlowStatus { IDLE, INITIATING, WAITING_BROWSER, POLLING, SUCCESS, ERROR }
enum class PinSetupStep { ENTER_NEW, CONFIRM }

data class SettingsUiState(
    // ── CalDAV ──────────────────────────────────────────────────────────────
    val authMethod: CalDavAuthMethod = CalDavAuthMethod.MANUAL,
    val caldavServer: String = "",
    val caldavUsername: String = "",
    val caldavPassword: String = "",
    val caldavConnectionStatus: CalDavConnectionStatus = CalDavConnectionStatus.IDLE,
    val caldavConnectionMessage: String = "",
    /** SSO account name when authMethod == NEXTCLOUD_SSO */
    val ssoAccountName: String? = null,
    val isNextcloudAppInstalled: Boolean = false,
    /** Login Flow v2 state */
    val loginFlowStatus: NextcloudFlowStatus = NextcloudFlowStatus.IDLE,
    /** URL to open in the browser for Login Flow v2 */
    val loginFlowUrl: String? = null,
    val loginFlowError: String? = null,

    // ── Calendar picker ──────────────────────────────────────────────────────
    val availableCalendars: List<CalDavCalendar> = emptyList(),
    val selectedCalendarHrefs: Set<String> = emptySet(),
    val defaultEventCalendar: String? = null,
    val defaultTaskCalendar: String? = null,
    val isLoadingCalendars: Boolean = false,

    // ── Gitea ─────────────────────────────────────────────────────────────────
    val giteaInstance: String = "",
    val giteaToken: String = "",
    val giteaRepos: String = "",        // comma-separated "owner/repo"

    // ── Appearance ────────────────────────────────────────────────────────────
    val theme: ThemePreference = ThemePreference.SYSTEM,

    // ── Navigation ───────────────────────────────────────────────────────────
    val visibleScreens: List<String> = ALL_SCREENS,

    // ── Task defaults ─────────────────────────────────────────────────────────
    val taskDefaults: TaskDefaults = TaskDefaults(),

    // ── Pomodoro ─────────────────────────────────────────────────────────────
    val pomodoroSettings: PomodoroSettings = PomodoroSettings(),

    // ── Notifications ────────────────────────────────────────────────────────
    val notificationsEnabled: Boolean = true,
    val notificationMinutes: Int = 15,

    // ── Widget sync ───────────────────────────────────────────────────────────
    val widgetSyncInterval: Int = 60,
    val isSyncingNow: Boolean = false,

    // ── Security ──────────────────────────────────────────────────────────────
    /** Master toggle: whether the app PIN lock is active. */
    val pinLockEnabled: Boolean = false,
    /** Convenience option: show a biometric / device-credential button on the lock screen.
     *  Only available (and relevant) when [pinLockEnabled] is true. */
    val systemCredentialEnabled: Boolean = false,
    /** PIN setup dialog state */
    val showPinSetupDialog: Boolean = false,
    val pinSetupStep: PinSetupStep = PinSetupStep.ENTER_NEW,
    /** First PIN entry (held while user enters confirmation) */
    val pinSetupFirst: String = "",
    /** Digits entered so far in the current step */
    val pinSetupCurrent: String = "",
    val pinSetupError: String? = null,

    // ── General ───────────────────────────────────────────────────────────────
    val infoMessage: String? = null,
    val errorMessage: String? = null,
)

@HiltViewModel
class SettingsViewModel @Inject constructor(
    @ApplicationContext private val context: Context,
    private val secureStore: SecureStore,
    private val prefs: AppPreferences,
    private val calDavClient: CalDavClient,
    private val ssoHelper: NextcloudSsoHelper,
    private val loginFlow: NextcloudLoginFlow,
    private val workManager: WorkManager,
    private val colorResolver: CalendarColorResolver,
    private val ssoResultBus: SsoResultBus,
) : ViewModel() {

    private val _state = MutableStateFlow(SettingsUiState())
    val state: StateFlow<SettingsUiState> = _state.asStateFlow()

    private val json = Json { ignoreUnknownKeys = true }

    init {
        loadPersistedSettings()
        collectPrefsFlows()
        collectSsoResults()
    }

    private fun collectSsoResults() {
        viewModelScope.launch {
            ssoResultBus.accountName.collect { name -> onSsoResult(name) }
        }
    }

    // ─── Init: load all persisted values ─────────────────────────────────────

    private fun loadPersistedSettings() {
        viewModelScope.launch {
            val authRaw = secureStore.get(CredentialKey.CALDAV_AUTH_METHOD)
            val authMethod = runCatching {
                if (authRaw != null) CalDavAuthMethod.valueOf(authRaw) else CalDavAuthMethod.MANUAL
            }.getOrDefault(CalDavAuthMethod.MANUAL)

            val ssoAccount = ssoHelper.getStoredAccount()
            val selectedJson = secureStore.get(CredentialKey.CALDAV_SELECTED_CALENDARS)
            val selected = selectedJson?.let {
                runCatching { json.decodeFromString<List<CalDavCalendar>>(it) }.getOrNull()
            } ?: emptyList()

            _state.update { s ->
                s.copy(
                    authMethod = authMethod,
                    caldavServer = secureStore.get(CredentialKey.CALDAV_SERVER) ?: "",
                    caldavUsername = secureStore.get(CredentialKey.CALDAV_USERNAME) ?: "",
                    caldavPassword = secureStore.get(CredentialKey.CALDAV_PASSWORD) ?: "",
                    ssoAccountName = ssoAccount?.name,
                    isNextcloudAppInstalled = ssoHelper.isNextcloudAppInstalled(),
                    availableCalendars = selected,
                    selectedCalendarHrefs = selected.map { it.href }.toSet(),
                    defaultEventCalendar = secureStore.get(CredentialKey.CALDAV_DEFAULT_EVENT_CALENDAR),
                    defaultTaskCalendar = secureStore.get(CredentialKey.CALDAV_DEFAULT_TASK_CALENDAR),
                    giteaInstance = secureStore.get(CredentialKey.GITEA_INSTANCE) ?: "",
                    giteaToken = secureStore.get(CredentialKey.GITEA_TOKEN) ?: "",
                    giteaRepos = secureStore.get(CredentialKey.GITEA_REPOS) ?: "",
                )
            }
        }
    }

    private fun collectPrefsFlows() {
        viewModelScope.launch {
            prefs.themeFlow.collect { t -> _state.update { it.copy(theme = t) } }
        }
        viewModelScope.launch {
            prefs.visibleScreensFlow.collect { s -> _state.update { it.copy(visibleScreens = s) } }
        }
        viewModelScope.launch {
            prefs.taskDefaultsFlow.collect { d -> _state.update { it.copy(taskDefaults = d) } }
        }
        viewModelScope.launch {
            prefs.pomodoroSettingsFlow.collect { p -> _state.update { it.copy(pomodoroSettings = p) } }
        }
        viewModelScope.launch {
            prefs.notificationsFlow.collect { (enabled, min) ->
                _state.update { it.copy(notificationsEnabled = enabled, notificationMinutes = min) }
            }
        }
        viewModelScope.launch {
            prefs.widgetSyncIntervalFlow.collect { i -> _state.update { it.copy(widgetSyncInterval = i) } }
        }
        viewModelScope.launch {
            prefs.biometricLockFlow.collect { b -> _state.update { it.copy(pinLockEnabled = b) } }
        }
        viewModelScope.launch {
            prefs.systemCredentialFlow.collect { b -> _state.update { it.copy(systemCredentialEnabled = b) } }
        }
    }

    // ─── CalDAV section ───────────────────────────────────────────────────────

    fun setAuthMethod(method: CalDavAuthMethod) {
        _state.update { it.copy(authMethod = method) }
    }

    fun setCalDavServer(v: String) = _state.update { it.copy(caldavServer = v) }
    fun setCalDavUsername(v: String) = _state.update { it.copy(caldavUsername = v) }
    fun setCalDavPassword(v: String) = _state.update { it.copy(caldavPassword = v) }

    fun saveCalDav() {
        val s = _state.value
        secureStore.set(CredentialKey.CALDAV_AUTH_METHOD, s.authMethod.name)
        secureStore.set(CredentialKey.CALDAV_SERVER, s.caldavServer)
        secureStore.set(CredentialKey.CALDAV_USERNAME, s.caldavUsername)
        secureStore.set(CredentialKey.CALDAV_PASSWORD, s.caldavPassword)
        _state.update { it.copy(infoMessage = "CalDAV settings saved") }
    }

    fun testCalDavConnection() {
        viewModelScope.launch {
            _state.update { it.copy(caldavConnectionStatus = CalDavConnectionStatus.TESTING) }
            val result = calDavClient.testConnection()
            result.onSuccess { msg ->
                _state.update { it.copy(caldavConnectionStatus = CalDavConnectionStatus.SUCCESS, caldavConnectionMessage = msg) }
            }.onFailure { e ->
                _state.update { it.copy(caldavConnectionStatus = CalDavConnectionStatus.ERROR, caldavConnectionMessage = e.message ?: "Connection failed") }
            }
        }
    }

    // ─── Nextcloud SSO ────────────────────────────────────────────────────────

    /**
     * Returns the Intent to pass to [ActivityResultLauncher.launch] for SSO.
     * The caller (SettingsScreen) must hold a [rememberLauncherForActivityResult] and
     * call [onSsoResult] in the callback.
     */
    fun buildSsoPickerIntent() = ssoHelper.buildPickAccountIntent(context)

    /**
     * Called from [rememberLauncherForActivityResult] callback after the SSO picker returns.
     * [accountName] is the Nextcloud account name that was selected (or null on failure).
     */
    fun onSsoResult(accountName: String?) {
        if (accountName == null) {
            _state.update { it.copy(errorMessage = "Nextcloud SSO sign-in was cancelled") }
            return
        }
        ssoHelper.commitAccount(accountName)
        secureStore.set(CredentialKey.CALDAV_AUTH_METHOD, CalDavAuthMethod.NEXTCLOUD_SSO.name)
        secureStore.set(CredentialKey.NEXTCLOUD_SSO_ACCOUNT, accountName)

        // The Nextcloud SSO token is only usable via the AIDL proxy, but the
        // SingleSignOnAccount also carries the plain app-password (token field) which
        // is a valid Basic-auth password for CalDAV. Extract it so CalDavClient works.
        val ssoAccount = ssoHelper.getStoredAccount()
        if (ssoAccount != null) {
            secureStore.set(CredentialKey.CALDAV_SERVER, ssoAccount.url)
            secureStore.set(CredentialKey.CALDAV_USERNAME, ssoAccount.userId)
            secureStore.set(CredentialKey.CALDAV_PASSWORD, ssoAccount.token)
            _state.update {
                it.copy(
                    ssoAccountName = accountName,
                    authMethod = CalDavAuthMethod.NEXTCLOUD_SSO,
                    caldavServer = ssoAccount.url,
                    caldavUsername = ssoAccount.userId,
                    infoMessage = "Signed in via Nextcloud SSO",
                )
            }
        } else {
            _state.update {
                it.copy(
                    ssoAccountName = accountName,
                    authMethod = CalDavAuthMethod.NEXTCLOUD_SSO,
                    infoMessage = "Signed in via Nextcloud SSO",
                )
            }
        }
        // Automatically discover calendars so the picker appears right away
        refreshCalendars()
    }

    fun clearSsoAccount() {
        ssoHelper.clearAccount()
        secureStore.delete(CredentialKey.NEXTCLOUD_SSO_ACCOUNT)
        _state.update { it.copy(ssoAccountName = null) }
    }

    // ─── Nextcloud Login Flow v2 ──────────────────────────────────────────────

    fun startLoginFlow(serverUrl: String) {
        viewModelScope.launch {
            _state.update { it.copy(loginFlowStatus = NextcloudFlowStatus.INITIATING, loginFlowError = null) }
            loginFlow.initiate(serverUrl).onSuccess { init ->
                _state.update { it.copy(
                    loginFlowStatus = NextcloudFlowStatus.WAITING_BROWSER,
                    loginFlowUrl = init.loginUrl,
                ) }
                // Begin polling in background
                pollLoginFlow(init.pollEndpoint, init.pollToken)
            }.onFailure { e ->
                _state.update { it.copy(loginFlowStatus = NextcloudFlowStatus.ERROR, loginFlowError = e.message) }
            }
        }
    }

    private fun pollLoginFlow(pollEndpoint: String, pollToken: String) {
        viewModelScope.launch {
            _state.update { it.copy(loginFlowStatus = NextcloudFlowStatus.POLLING) }
            loginFlow.poll(pollEndpoint, pollToken).onSuccess { creds ->
                secureStore.set(CredentialKey.CALDAV_SERVER, creds.serverUrl)
                secureStore.set(CredentialKey.CALDAV_USERNAME, creds.loginName)
                secureStore.set(CredentialKey.CALDAV_PASSWORD, creds.appPassword)
                secureStore.set(CredentialKey.CALDAV_AUTH_METHOD, CalDavAuthMethod.LOGIN_FLOW_V2.name)
                _state.update { it.copy(
                    loginFlowStatus = NextcloudFlowStatus.SUCCESS,
                    loginFlowUrl = null,
                    caldavServer = creds.serverUrl,
                    caldavUsername = creds.loginName,
                    caldavPassword = creds.appPassword,
                    authMethod = CalDavAuthMethod.LOGIN_FLOW_V2,
                    infoMessage = "Logged in via Nextcloud — credentials saved",
                ) }
                // Automatically discover calendars so the picker appears right away
                refreshCalendars()
            }.onFailure { e ->
                _state.update { it.copy(loginFlowStatus = NextcloudFlowStatus.ERROR, loginFlowError = e.message) }
            }
        }
    }

    fun cancelLoginFlow() {
        _state.update { it.copy(loginFlowStatus = NextcloudFlowStatus.IDLE, loginFlowUrl = null, loginFlowError = null) }
    }

    // ─── Calendar picker ──────────────────────────────────────────────────────

    fun refreshCalendars() {
        viewModelScope.launch {
            _state.update { it.copy(isLoadingCalendars = true) }
            try {
                val cals = calDavClient.fetchCalendars()
                // Preserve existing selection for cals that are still available
                val currentHrefs = _state.value.selectedCalendarHrefs
                val newSelected = cals.filter { it.href in currentHrefs }.map { it.href }.toSet()
                    .ifEmpty { cals.map { it.href }.toSet() }
                _state.update { it.copy(
                    availableCalendars = cals,
                    selectedCalendarHrefs = newSelected,
                    isLoadingCalendars = false,
                ) }
            } catch (e: Exception) {
                _state.update { it.copy(isLoadingCalendars = false, errorMessage = e.message) }
            }
        }
    }

    fun toggleCalendarSelection(href: String) {
        val current = _state.value.selectedCalendarHrefs
        val updated = if (href in current) current - href else current + href
        // Keep at least one selected
        if (updated.isEmpty()) return
        _state.update { it.copy(selectedCalendarHrefs = updated) }
    }

    fun selectAllCalendars() {
        val allHrefs = _state.value.availableCalendars.map { it.href }.toSet()
        _state.update { it.copy(selectedCalendarHrefs = allHrefs) }
    }

    fun deselectAllCalendars() {
        // Keep at least the first one
        val first = _state.value.availableCalendars.firstOrNull()?.href
        _state.update { it.copy(selectedCalendarHrefs = setOfNotNull(first)) }
    }

    fun saveCalendarSelection() {
        val s = _state.value
        val selected = s.availableCalendars.filter { it.href in s.selectedCalendarHrefs }
        secureStore.set(CredentialKey.CALDAV_SELECTED_CALENDARS, json.encodeToString(selected))
        colorResolver.invalidate()
        _state.update { it.copy(infoMessage = "Calendar selection saved") }
    }

    fun setDefaultEventCalendar(href: String) {
        secureStore.set(CredentialKey.CALDAV_DEFAULT_EVENT_CALENDAR, href)
        _state.update { it.copy(defaultEventCalendar = href) }
    }

    fun setDefaultTaskCalendar(href: String) {
        secureStore.set(CredentialKey.CALDAV_DEFAULT_TASK_CALENDAR, href)
        _state.update { it.copy(defaultTaskCalendar = href) }
    }

    // ─── Gitea section ────────────────────────────────────────────────────────

    fun setGiteaInstance(v: String) = _state.update { it.copy(giteaInstance = v) }
    fun setGiteaToken(v: String) = _state.update { it.copy(giteaToken = v) }
    fun setGiteaRepos(v: String) = _state.update { it.copy(giteaRepos = v) }

    fun saveGitea() {
        val s = _state.value
        secureStore.set(CredentialKey.GITEA_INSTANCE, s.giteaInstance)
        secureStore.set(CredentialKey.GITEA_TOKEN, s.giteaToken)
        secureStore.set(CredentialKey.GITEA_REPOS, s.giteaRepos)
        _state.update { it.copy(infoMessage = "Gitea settings saved") }
    }

    // ─── Appearance ───────────────────────────────────────────────────────────

    fun setTheme(theme: ThemePreference) {
        viewModelScope.launch { prefs.setTheme(theme) }
    }

    // ─── Navigation ───────────────────────────────────────────────────────────

    fun toggleVisibleScreen(screen: String) {
        val current = _state.value.visibleScreens.toMutableList()
        if (screen in current) {
            if (current.size <= 1) return // keep at least one
            current.remove(screen)
        } else {
            current.add(screen)
        }
        viewModelScope.launch { prefs.setVisibleScreens(current) }
    }

    // ─── Task defaults ────────────────────────────────────────────────────────

    fun setTaskDefaults(d: TaskDefaults) {
        viewModelScope.launch { prefs.setTaskDefaults(d) }
    }

    // ─── Pomodoro ─────────────────────────────────────────────────────────────

    fun setPomodoroSettings(s: PomodoroSettings) {
        viewModelScope.launch { prefs.setPomodoroSettings(s) }
    }

    // ─── Notifications ────────────────────────────────────────────────────────

    fun setNotifications(enabled: Boolean, minutesBefore: Int) {
        viewModelScope.launch { prefs.setNotifications(enabled, minutesBefore) }
    }

    // ─── Widget sync ──────────────────────────────────────────────────────────

    fun setWidgetSyncInterval(minutes: Int) {
        val clamped = minutes.coerceAtLeast(15)
        viewModelScope.launch {
            prefs.setWidgetSyncInterval(clamped)
            // Re-schedule with new interval
            workManager.enqueueUniquePeriodicWork(
                SyncWorker.WORK_NAME_PERIODIC,
                ExistingPeriodicWorkPolicy.UPDATE,
                SyncWorker.periodicRequest(clamped),
            )
        }
    }

    fun syncNow() {
        viewModelScope.launch {
            _state.update { it.copy(isSyncingNow = true) }
            workManager.enqueueUniqueWork(
                SyncWorker.WORK_NAME_ONCE,
                ExistingWorkPolicy.REPLACE,
                SyncWorker.oneTimeRequest(),
            )
            // The worker running asynchronously — just reset the loading flag after enqueueing
            _state.update { it.copy(isSyncingNow = false, infoMessage = "Sync started in background") }
        }
    }

    // ─── PIN lock & system credential ────────────────────────────────────────

    /** Called when the user flips the PIN lock toggle ON — opens the PIN setup dialog. */
    fun requestEnableLock() {
        _state.update {
            it.copy(
                showPinSetupDialog = true,
                pinSetupStep = PinSetupStep.ENTER_NEW,
                pinSetupFirst = "",
                pinSetupCurrent = "",
                pinSetupError = null,
            )
        }
    }

    /** Disable the PIN lock (and the system-credential convenience at the same time). */
    fun disableLock() {
        viewModelScope.launch {
            prefs.setBiometricLock(false)
            prefs.setSystemCredential(false)   // no point keeping this on without a PIN
        }
    }

    /** Toggle the system-credential (biometric) convenience option on/off. */
    fun setSystemCredential(enabled: Boolean) {
        viewModelScope.launch { prefs.setSystemCredential(enabled) }
    }

    fun onPinSetupDigit(digit: String) {
        val current = _state.value.pinSetupCurrent
        if (current.length >= 6) return
        val updated = current + digit
        _state.update { it.copy(pinSetupCurrent = updated, pinSetupError = null) }
        if (updated.length == 6) {
            when (_state.value.pinSetupStep) {
                PinSetupStep.ENTER_NEW -> {
                    // Move to confirmation step
                    _state.update {
                        it.copy(
                            pinSetupStep = PinSetupStep.CONFIRM,
                            pinSetupFirst = updated,
                            pinSetupCurrent = "",
                        )
                    }
                }
                PinSetupStep.CONFIRM -> {
                    if (updated == _state.value.pinSetupFirst) {
                        savePinAndEnableLock(updated)
                    } else {
                        // Mismatch — restart from step 1
                        _state.update {
                            it.copy(
                                pinSetupStep = PinSetupStep.ENTER_NEW,
                                pinSetupFirst = "",
                                pinSetupCurrent = "",
                                pinSetupError = "PINs don't match — please try again",
                            )
                        }
                    }
                }
            }
        }
    }

    fun onPinSetupDelete() {
        _state.update { it.copy(pinSetupCurrent = it.pinSetupCurrent.dropLast(1), pinSetupError = null) }
    }

    fun cancelPinSetup() {
        _state.update {
            it.copy(showPinSetupDialog = false, pinSetupCurrent = "", pinSetupFirst = "", pinSetupError = null)
        }
    }

    private fun savePinAndEnableLock(pin: String) {
        viewModelScope.launch {
            prefs.setPinHash(sha256(pin))
            prefs.setBiometricLock(true)
            _state.update {
                it.copy(showPinSetupDialog = false, pinSetupCurrent = "", pinSetupFirst = "", infoMessage = "App lock enabled")
            }
        }
    }

    private fun sha256(input: String): String {
        val bytes = java.security.MessageDigest.getInstance("SHA-256").digest(input.toByteArray())
        return bytes.joinToString("") { "%02x".format(it) }
    }

    // ─── Dismiss messages ─────────────────────────────────────────────────────

    fun dismissInfo() = _state.update { it.copy(infoMessage = null) }
    fun dismissError() = _state.update { it.copy(errorMessage = null) }
}
