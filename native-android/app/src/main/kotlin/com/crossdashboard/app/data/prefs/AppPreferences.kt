package com.crossdashboard.app.data.prefs

import android.content.Context
import androidx.datastore.core.DataStore
import androidx.datastore.preferences.core.*
import androidx.datastore.preferences.preferencesDataStore
import com.crossdashboard.app.domain.model.*
import dagger.hilt.android.qualifiers.ApplicationContext
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.catch
import kotlinx.coroutines.flow.map
import kotlinx.serialization.encodeToString
import kotlinx.serialization.json.Json
import java.io.IOException
import javax.inject.Inject
import javax.inject.Singleton

private val Context.dataStore: DataStore<Preferences> by preferencesDataStore(name = "app_prefs")

@Singleton
class AppPreferences @Inject constructor(
    @param:ApplicationContext private val context: Context,
) {
    private val ds = context.dataStore

    // ─── Theme ───────────────────────────────────────────────────────────────

    val themeFlow: Flow<ThemePreference> = ds.data
        .catchIo()
        .map { p -> ThemePreference.valueOf(p[Keys.THEME] ?: ThemePreference.SYSTEM.name) }

    suspend fun setTheme(theme: ThemePreference) {
        ds.edit { it[Keys.THEME] = theme.name }
    }

    // ─── Timezone ────────────────────────────────────────────────────────────

    /** Null means automatic: use the operating-system timezone. */
    val timeZoneOverrideFlow: Flow<String?> = ds.data
        .catchIo()
        .map { p -> p[Keys.TIME_ZONE_OVERRIDE]?.takeIf { it.isNotBlank() } }

    suspend fun setTimeZoneOverride(zoneId: String?) {
        val normalized = zoneId?.trim()?.takeIf { it.isNotEmpty() }
        ds.edit { p ->
            if (normalized == null) p.remove(Keys.TIME_ZONE_OVERRIDE)
            else p[Keys.TIME_ZONE_OVERRIDE] = normalized
        }
        AppTimeZone.applyOverride(normalized)
    }

    // ─── Visible screens ─────────────────────────────────────────────────────

    val visibleScreensFlow: Flow<List<String>> = ds.data
        .catchIo()
        .map { p ->
            p[Keys.VISIBLE_SCREENS]
                ?.split(",")
                ?.filter { it.isNotBlank() }
                ?: ALL_SCREENS
        }

    suspend fun setVisibleScreens(screens: List<String>) {
        ds.edit { it[Keys.VISIBLE_SCREENS] = screens.joinToString(",") }
    }

    // ─── Kanban columns ──────────────────────────────────────────────────────

    val kanbanColumnsFlow: Flow<List<String>> = ds.data
        .catchIo()
        .map { p ->
            p[Keys.KANBAN_COLUMNS]
                ?.split(",")
                ?.filter { it.isNotBlank() }
                ?: DEFAULT_KANBAN_COLUMNS
        }

    suspend fun setKanbanColumns(columns: List<String>) {
        ds.edit { it[Keys.KANBAN_COLUMNS] = columns.joinToString(",") }
    }

    // ─── Pomodoro ────────────────────────────────────────────────────────────

    val pomodoroSettingsFlow: Flow<PomodoroSettings> = ds.data
        .catchIo()
        .map { p ->
            PomodoroSettings(
                workMinutes = p[Keys.POMODORO_WORK] ?: 25,
                shortBreakMinutes = p[Keys.POMODORO_SHORT] ?: 5,
                longBreakMinutes = p[Keys.POMODORO_LONG] ?: 15,
                sessionsUntilLongBreak = p[Keys.POMODORO_SESSIONS] ?: 4,
            )
        }

    suspend fun setPomodoroSettings(s: PomodoroSettings) {
        ds.edit {
            it[Keys.POMODORO_WORK] = s.workMinutes
            it[Keys.POMODORO_SHORT] = s.shortBreakMinutes
            it[Keys.POMODORO_LONG] = s.longBreakMinutes
            it[Keys.POMODORO_SESSIONS] = s.sessionsUntilLongBreak
        }
    }

    // ─── Task input defaults ─────────────────────────────────────────────────

    val taskDefaultsFlow: Flow<TaskDefaults> = ds.data
        .catchIo()
        .map { p ->
            TaskDefaults(
                morningHour = p[Keys.TASK_MORNING_HOUR] ?: 8,
                afternoonHour = p[Keys.TASK_AFTERNOON_HOUR] ?: 13,
                nightHour = p[Keys.TASK_NIGHT_HOUR] ?: 21,
                defaultHour = p[Keys.TASK_DEFAULT_HOUR] ?: 10,
            )
        }

    suspend fun setTaskDefaults(d: TaskDefaults) {
        ds.edit {
            it[Keys.TASK_MORNING_HOUR] = d.morningHour
            it[Keys.TASK_AFTERNOON_HOUR] = d.afternoonHour
            it[Keys.TASK_NIGHT_HOUR] = d.nightHour
            it[Keys.TASK_DEFAULT_HOUR] = d.defaultHour
        }
    }

    // ─── Notifications ───────────────────────────────────────────────────────

    val notificationsFlow: Flow<Pair<Boolean, Int>> = ds.data
        .catchIo()
        .map { p -> Pair(p[Keys.NOTIF_ENABLED] ?: true, p[Keys.NOTIF_MINUTES] ?: 15) }

    suspend fun setNotifications(enabled: Boolean, minutesBefore: Int) {
        ds.edit {
            it[Keys.NOTIF_ENABLED] = enabled
            it[Keys.NOTIF_MINUTES] = minutesBefore
        }
    }

    // ─── Widget sync interval ────────────────────────────────────────────────

    val widgetSyncIntervalFlow: Flow<Int> = ds.data
        .catchIo()
        .map { p -> p[Keys.WIDGET_SYNC_INTERVAL] ?: 60 }

    suspend fun setWidgetSyncInterval(minutes: Int) {
        ds.edit { it[Keys.WIDGET_SYNC_INTERVAL] = minutes.coerceAtLeast(15) }
    }

    // ─── PIN lock ────────────────────────────────────────────────────────────

    /** Whether the app PIN lock is active (user must enter their custom PIN to open the app). */
    val biometricLockFlow: Flow<Boolean> = ds.data
        .catchIo()
        .map { p -> p[Keys.BIOMETRIC_LOCK] ?: false }

    val biometricPinHashFlow: Flow<String?> = ds.data
        .catchIo()
        .map { p -> p[Keys.BIOMETRIC_PIN_HASH] }

    /**
     * Whether the system-credential / biometric convenience option is enabled.
     * Only meaningful when [biometricLockFlow] is also true.
     * When true, the lock screen shows a button the user can tap to authenticate
     * via fingerprint or device PIN instead of typing the app PIN.
     */
    val systemCredentialFlow: Flow<Boolean> = ds.data
        .catchIo()
        .map { p -> p[Keys.SYSTEM_CREDENTIAL] ?: false }

    suspend fun setBiometricLock(enabled: Boolean) {
        ds.edit { it[Keys.BIOMETRIC_LOCK] = enabled }
    }

    suspend fun setPinHash(hash: String) {
        ds.edit { it[Keys.BIOMETRIC_PIN_HASH] = hash }
    }

    suspend fun setSystemCredential(enabled: Boolean) {
        ds.edit { it[Keys.SYSTEM_CREDENTIAL] = enabled }
    }

    // ─── Last sync timestamp ─────────────────────────────────────────────────

    val lastSyncFlow: Flow<Long?> = ds.data
        .catchIo()
        .map { p -> p[Keys.LAST_SYNC_EPOCH] }

    suspend fun setLastSync(epochMillis: Long) {
        ds.edit { it[Keys.LAST_SYNC_EPOCH] = epochMillis }
    }

    // ─── Keys ─────────────────────────────────────────────────────────────────

    private object Keys {
        val THEME = stringPreferencesKey("theme")
        val TIME_ZONE_OVERRIDE = stringPreferencesKey("time_zone_override")
        val VISIBLE_SCREENS = stringPreferencesKey("visible_screens")
        val KANBAN_COLUMNS = stringPreferencesKey("kanban_columns")
        val POMODORO_WORK = intPreferencesKey("pomodoro_work")
        val POMODORO_SHORT = intPreferencesKey("pomodoro_short")
        val POMODORO_LONG = intPreferencesKey("pomodoro_long")
        val POMODORO_SESSIONS = intPreferencesKey("pomodoro_sessions")
        val TASK_MORNING_HOUR = intPreferencesKey("task_morning_hour")
        val TASK_AFTERNOON_HOUR = intPreferencesKey("task_afternoon_hour")
        val TASK_NIGHT_HOUR = intPreferencesKey("task_night_hour")
        val TASK_DEFAULT_HOUR = intPreferencesKey("task_default_hour")
        val NOTIF_ENABLED = booleanPreferencesKey("notif_enabled")
        val NOTIF_MINUTES = intPreferencesKey("notif_minutes")
        val WIDGET_SYNC_INTERVAL = intPreferencesKey("widget_sync_interval")
        val BIOMETRIC_LOCK = booleanPreferencesKey("biometric_lock")
        val BIOMETRIC_PIN_HASH = stringPreferencesKey("biometric_pin_hash")
        val SYSTEM_CREDENTIAL = booleanPreferencesKey("system_credential")
        val LAST_SYNC_EPOCH = longPreferencesKey("last_sync_epoch")
    }
}

@Suppress("UNCHECKED_CAST")
private fun <T> Flow<T>.catchIo(): Flow<T> = catch { e ->
    if (e is IOException) emit(emptyPreferences() as T) else throw e
}
