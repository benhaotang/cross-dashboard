import React, { useState, useEffect, useRef } from 'react';
import {
  View,
  Text,
  StyleSheet,
  TextInput,
  TouchableOpacity,
  ScrollView,
  Alert,
  Platform,
  Linking,
  ActivityIndicator,
} from 'react-native';
import { useApp } from '../store/AppContext';
import { useTheme } from '../hooks/useTheme';
import { ThemePreference } from '../theme';
import { CalDavCalendar } from '../types';
import * as keyring from '../services/keyring';
import * as caldav from '../services/caldav';
import * as nextcloud from '../services/nextcloud';
import * as gitea from '../services/gitea';
import * as cache from '../services/cache';
import * as crypto from '../services/crypto';
import * as notifications from '../services/notifications';
import { DEFAULT_TASK_DEFAULTS } from '../services/taskParser';
import { DEFAULT_POMODORO } from '../services/cache';

type AuthMethod = 'manual' | 'nextcloud';

export default function SettingsScreen() {
  const { setCaldavConfigured, setGiteaConfigured, setGiteaRepositories, setSelectedCalendars, setThemePreference, setLastSync, state } =
    useApp();
  const theme = useTheme();

  // CalDAV settings
  const [authMethod, setAuthMethod] = useState<AuthMethod>('manual');
  const [caldavServer, setCaldavServer] = useState('');
  const [caldavUsername, setCaldavUsername] = useState('');
  const [caldavPassword, setCaldavPassword] = useState('');
  const [ncServer, setNcServer] = useState('');
  const [ncLogging, setNcLogging] = useState(false);
  const ncAbort = useRef<AbortController | null>(null);

  // Calendar picker
  const [availableCalendars, setAvailableCalendars] = useState<CalDavCalendar[]>([]);
  const [calendarSelection, setCalendarSelection] = useState<Set<string>>(new Set());
  const [fetchingCalendars, setFetchingCalendars] = useState(false);
  const [defaultEventCalendar, setDefaultEventCalendar] = useState<string>('');
  const [defaultTaskCalendar, setDefaultTaskCalendar] = useState<string>('');

  // Gitea settings
  const [giteaInstance, setGiteaInstance] = useState('');
  const [giteaToken, setGiteaToken] = useState('');
  const [giteaRepos, setGiteaRepos] = useState('');

  // Notification settings
  const [notifEnabled, setNotifEnabled] = useState(false);
  const [reminderMinutes, setReminderMinutes] = useState('15');

  // UnifiedPush settings
  const [upAvailable, setUpAvailable] = useState(false);
  const [upDistributors, setUpDistributors] = useState<string[]>([]);
  const [upDistributor, setUpDistributor] = useState('');
  const [upEndpoint, setUpEndpoint] = useState<string | null>(null);
  const [upRegistered, setUpRegistered] = useState(false);

  // Security / Encryption
  const [isCustomKey, setIsCustomKey] = useState(false);
  const [passphrase, setPassphrase] = useState('');
  const [securityBusy, setSecurityBusy] = useState(false);

  // Task input defaults
  const [taskMorning, setTaskMorning] = useState(String(DEFAULT_TASK_DEFAULTS.morningHour));
  const [taskAfternoon, setTaskAfternoon] = useState(String(DEFAULT_TASK_DEFAULTS.afternoonHour));
  const [taskNight, setTaskNight] = useState(String(DEFAULT_TASK_DEFAULTS.nightHour));
  const [taskDefault, setTaskDefault] = useState(String(DEFAULT_TASK_DEFAULTS.defaultHour));

  // Pomodoro settings
  const [pomWork, setPomWork] = useState(String(DEFAULT_POMODORO.workMinutes));
  const [pomShortBreak, setPomShortBreak] = useState(String(DEFAULT_POMODORO.shortBreakMinutes));
  const [pomLongBreak, setPomLongBreak] = useState(String(DEFAULT_POMODORO.longBreakMinutes));
  const [pomSessions, setPomSessions] = useState(String(DEFAULT_POMODORO.sessionsUntilLongBreak));

  // Status
  const [caldavStatus, setCaldavStatus] = useState<'unknown' | 'testing' | 'success' | 'error'>('unknown');
  const [giteaStatus, setGiteaStatus] = useState<'unknown' | 'testing' | 'success' | 'error'>('unknown');

  useEffect(() => {
    loadSettings();
    loadNotificationSettings();
    loadTaskDefaults();
    loadPomodoroDefaults();
  }, []);

  // Listen for UP endpoint changes
  useEffect(() => {
    const cleanup = notifications.onUPEndpoint((endpoint, _instance) => {
      setUpEndpoint(endpoint);
      setUpRegistered(true);
      notifications.saveUPEndpoint(endpoint);
      showAlert('UnifiedPush', `Registered! Endpoint:\n${endpoint}`);
    });
    const cleanupFail = notifications.onUPRegistrationFailed((_instance) => {
      setUpRegistered(false);
      showAlert('UnifiedPush', 'Registration failed. Is the distributor running?');
    });
    const cleanupUnreg = notifications.onUPUnregistered((_instance) => {
      setUpRegistered(false);
      setUpEndpoint(null);
    });
    const cleanupMsg = notifications.onUPMessage((_message, _instance) => {
      // Message is auto-displayed as notification by the service
    });

    return () => {
      cleanup?.();
      cleanupFail?.();
      cleanupUnreg?.();
      cleanupMsg?.();
    };
  }, []);

  async function loadSettings() {
    const [server, username, instance, customKey, savedAuthMethod, savedCalendars, savedDefEvent, savedDefTask] = await Promise.all([
      keyring.getCredential('caldav_server'),
      keyring.getCredential('caldav_username'),
      keyring.getCredential('gitea_instance'),
      crypto.hasCustomKey(),
      keyring.getCredential('caldav_auth_method'),
      keyring.getCredential('caldav_selected_calendars'),
      keyring.getCredential('caldav_default_event_calendar'),
      keyring.getCredential('caldav_default_task_calendar'),
    ]);

    if (server) setCaldavServer(server);
    if (username) setCaldavUsername(username);
    if (instance) setGiteaInstance(instance);
    if (state.giteaRepositories.length > 0) setGiteaRepos(state.giteaRepositories.join('\n'));
    setIsCustomKey(customKey);
    if (savedAuthMethod === 'nextcloud' || savedAuthMethod === 'manual') {
      setAuthMethod(savedAuthMethod);
    }
    if (savedCalendars) {
      try {
        const cals: CalDavCalendar[] = JSON.parse(savedCalendars);
        setSelectedCalendars(cals);
        setCalendarSelection(new Set(cals.map((c) => c.href)));
      } catch { /* ignore parse errors */ }
    }
    if (savedDefEvent) setDefaultEventCalendar(savedDefEvent);
    if (savedDefTask) setDefaultTaskCalendar(savedDefTask);
  }

  async function loadNotificationSettings() {
    const settings = await notifications.getNotificationSettings();
    setNotifEnabled(settings.enabled);
    setReminderMinutes(String(settings.minutesBefore));
    if (settings.upEndpoint) {
      setUpEndpoint(settings.upEndpoint);
      setUpRegistered(true);
    }

    // Check UP availability (Android only)
    const available = notifications.isUnifiedPushAvailable();
    setUpAvailable(available);
    if (available) {
      const distributors = notifications.getUPDistributors();
      setUpDistributors(distributors);
      const current = notifications.getUPDistributor();
      if (current) setUpDistributor(current);
    }
  }

  async function loadTaskDefaults() {
    const saved = await cache.loadTaskDefaults();
    if (saved) {
      setTaskMorning(String(saved.morningHour));
      setTaskAfternoon(String(saved.afternoonHour));
      setTaskNight(String(saved.nightHour));
      setTaskDefault(String(saved.defaultHour));
    }
  }

  async function saveTaskDefaultsHandler() {
    const defaults = {
      morningHour: Math.max(0, Math.min(23, parseInt(taskMorning, 10) || DEFAULT_TASK_DEFAULTS.morningHour)),
      afternoonHour: Math.max(0, Math.min(23, parseInt(taskAfternoon, 10) || DEFAULT_TASK_DEFAULTS.afternoonHour)),
      nightHour: Math.max(0, Math.min(23, parseInt(taskNight, 10) || DEFAULT_TASK_DEFAULTS.nightHour)),
      defaultHour: Math.max(0, Math.min(23, parseInt(taskDefault, 10) || DEFAULT_TASK_DEFAULTS.defaultHour)),
    };
    await cache.saveTaskDefaults(defaults);
    showAlert('Saved', 'Task input defaults updated');
  }

  async function loadPomodoroDefaults() {
    const saved = await cache.loadPomodoroSettings();
    if (saved) {
      setPomWork(String(saved.workMinutes));
      setPomShortBreak(String(saved.shortBreakMinutes));
      setPomLongBreak(String(saved.longBreakMinutes));
      setPomSessions(String(saved.sessionsUntilLongBreak));
    }
  }

  async function savePomodoroHandler() {
    const settings = {
      workMinutes: Math.max(1, Math.min(120, parseInt(pomWork, 10) || DEFAULT_POMODORO.workMinutes)),
      shortBreakMinutes: Math.max(1, Math.min(60, parseInt(pomShortBreak, 10) || DEFAULT_POMODORO.shortBreakMinutes)),
      longBreakMinutes: Math.max(1, Math.min(60, parseInt(pomLongBreak, 10) || DEFAULT_POMODORO.longBreakMinutes)),
      sessionsUntilLongBreak: Math.max(1, Math.min(12, parseInt(pomSessions, 10) || DEFAULT_POMODORO.sessionsUntilLongBreak)),
    };
    await cache.savePomodoroSettings(settings);
    showAlert('Saved', 'Pomodoro settings updated');
  }

  async function toggleNotifications() {
    const newValue = !notifEnabled;
    if (newValue) {
      const granted = await notifications.requestPermissions();
      if (!granted) {
        showAlert('Permissions', 'Notification permission not granted');
        return;
      }
    } else {
      await notifications.cancelAllEventReminders();
    }
    setNotifEnabled(newValue);
    await notifications.saveNotificationEnabled(newValue);

    if (newValue && state.events.length > 0) {
      const minutes = parseInt(reminderMinutes, 10) || 15;
      await notifications.scheduleEventReminders(state.events, minutes);
    }
  }

  async function saveReminderMinutes() {
    const minutes = parseInt(reminderMinutes, 10) || 15;
    await notifications.saveReminderMinutes(minutes);
    if (notifEnabled && state.events.length > 0) {
      await notifications.scheduleEventReminders(state.events, minutes);
    }
    showAlert('Saved', `Reminders set to ${minutes} minutes before events`);
  }

  function selectDistributor(distributor: string) {
    notifications.selectUPDistributor(distributor);
    setUpDistributor(distributor);
  }

  function registerUP() {
    if (!upDistributor) {
      showAlert('UnifiedPush', 'Please select a distributor first');
      return;
    }
    notifications.registerUnifiedPush('default');
  }

  function unregisterUP() {
    notifications.unregisterUnifiedPush('default');
    setUpRegistered(false);
    setUpEndpoint(null);
  }

  async function saveCalDav() {
    if (!caldavServer || !caldavUsername || !caldavPassword) {
      showAlert('Error', 'Please fill in all CalDAV fields');
      return;
    }

    await Promise.all([
      keyring.setCredential('caldav_server', caldavServer),
      keyring.setCredential('caldav_username', caldavUsername),
      keyring.setCredential('caldav_password', caldavPassword),
      keyring.setCredential('caldav_auth_method', authMethod),
    ]);

    setCaldavStatus('testing');
    const result = await caldav.testConnection();

    if (result.success) {
      setCaldavStatus('success');
      setCaldavConfigured(true);
      showAlert('Success', 'CalDAV connection successful!');
      loadCalendars();
    } else {
      setCaldavStatus('error');
      showAlert('Error', `CalDAV connection failed: ${result.error}`);
    }
  }

  async function startNextcloudLogin() {
    if (!ncServer.trim()) {
      showAlert('Error', 'Please enter your Nextcloud server URL');
      return;
    }

    if (Platform.OS === 'web') {
      showAlert('Not supported', 'Nextcloud Login Flow is not available on web due to CORS restrictions. Please use manual CalDAV entry with an app password instead.');
      return;
    }

    setNcLogging(true);
    const abort = new AbortController();
    ncAbort.current = abort;

    try {
      const flow = await nextcloud.initiateLoginFlow(ncServer.trim());
      await Linking.openURL(flow.loginUrl);
      const creds = await nextcloud.pollForCredentials(flow.pollEndpoint, flow.pollToken, abort.signal);

      const caldavUrl = nextcloud.discoverCalDavUrl(creds.server, creds.loginName);
      setCaldavServer(caldavUrl);
      setCaldavUsername(creds.loginName);
      setCaldavPassword(creds.appPassword);

      await Promise.all([
        keyring.setCredential('caldav_server', caldavUrl),
        keyring.setCredential('caldav_username', creds.loginName),
        keyring.setCredential('caldav_password', creds.appPassword),
        keyring.setCredential('caldav_auth_method', 'nextcloud'),
      ]);

      setCaldavStatus('testing');
      const result = await caldav.testConnection();
      if (result.success) {
        setCaldavStatus('success');
        setCaldavConfigured(true);
        showAlert('Success', `Logged in as ${creds.loginName}`);
        loadCalendars();
      } else {
        setCaldavStatus('error');
        showAlert('Error', `Connection test failed: ${result.error}`);
      }
    } catch (error) {
      if (!abort.signal.aborted) {
        showAlert('Error', error instanceof Error ? error.message : 'Login flow failed');
      }
    } finally {
      setNcLogging(false);
      ncAbort.current = null;
    }
  }

  function cancelNextcloudLogin() {
    ncAbort.current?.abort();
    setNcLogging(false);
  }

  async function loadCalendars() {
    setFetchingCalendars(true);
    try {
      const cals = await caldav.fetchCalendars();
      setAvailableCalendars(cals);
      // Pre-select all if no prior selection
      const savedRaw = await keyring.getCredential('caldav_selected_calendars');
      if (savedRaw) {
        try {
          const saved: CalDavCalendar[] = JSON.parse(savedRaw);
          setCalendarSelection(new Set(saved.map((c) => c.href)));
        } catch {
          setCalendarSelection(new Set(cals.map((c) => c.href)));
        }
      } else {
        setCalendarSelection(new Set(cals.map((c) => c.href)));
      }
    } catch {
      showAlert('Error', 'Failed to fetch calendar list');
    } finally {
      setFetchingCalendars(false);
    }
  }

  function toggleCalendarSelection(href: string) {
    setCalendarSelection((prev) => {
      const next = new Set(prev);
      if (next.has(href)) next.delete(href);
      else next.add(href);
      return next;
    });
  }

  function selectAllCalendars() {
    setCalendarSelection(new Set(availableCalendars.map((c) => c.href)));
  }

  function deselectAllCalendars() {
    setCalendarSelection(new Set());
  }

  async function saveCalendarSelection() {
    const selected = availableCalendars.filter((c) => calendarSelection.has(c.href));
    if (selected.length === 0) {
      showAlert('Warning', 'No calendars selected. Please select at least one calendar.');
      return;
    }
    await keyring.setCredential('caldav_selected_calendars', JSON.stringify(selected));
    setSelectedCalendars(selected);
    showAlert('Saved', `${selected.length} calendar(s) selected for sync`);
  }

  async function setDefaultEventCal(href: string) {
    setDefaultEventCalendar(href);
    await keyring.setCredential('caldav_default_event_calendar', href);
  }

  async function setDefaultTaskCal(href: string) {
    setDefaultTaskCalendar(href);
    await keyring.setCredential('caldav_default_task_calendar', href);
  }

  async function saveGitea() {
    if (!giteaInstance || !giteaToken) {
      showAlert('Error', 'Please fill in Gitea instance URL and token');
      return;
    }

    await Promise.all([
      keyring.setCredential('gitea_instance', giteaInstance),
      keyring.setCredential('gitea_token', giteaToken),
    ]);

    const repos = giteaRepos
      .split('\n')
      .map((r) => r.trim())
      .filter((r) => r.includes('/'));
    setGiteaRepositories(repos);

    setGiteaStatus('testing');
    const result = await gitea.testConnection();

    if (result.success) {
      setGiteaStatus('success');
      setGiteaConfigured(true);
      showAlert('Success', 'Gitea connection successful!');
    } else {
      setGiteaStatus('error');
      showAlert('Error', `Gitea connection failed: ${result.error}`);
    }
  }

  async function reEncryptCache(rotateKey: () => Promise<void>) {
    setSecurityBusy(true);
    try {
      const [events, notes, issues] = await Promise.all([
        cache.loadEvents(),
        cache.loadNotes(),
        cache.loadIssues(),
      ]);

      await rotateKey();

      await Promise.all([
        events ? cache.saveEvents(events) : Promise.resolve(),
        notes ? cache.saveNotes(notes) : Promise.resolve(),
        issues ? cache.saveIssues(issues) : Promise.resolve(),
      ]);
    } catch {
      await cache.clearCache();
      showAlert('Encryption', 'Re-encryption failed. Cached data has been cleared and will be re-fetched on next sync.');
    } finally {
      setSecurityBusy(false);
    }
  }

  async function handleSetCustomPassphrase() {
    if (passphrase.length < 8) {
      showAlert('Error', 'Passphrase must be at least 8 characters');
      return;
    }
    await reEncryptCache(() => crypto.setCustomKey(passphrase));
    setIsCustomKey(true);
    setPassphrase('');
    showAlert('Security', 'Custom passphrase set. Cache re-encrypted.');
  }

  async function handleResetToRandomKey() {
    await reEncryptCache(() => crypto.resetToRandomKey());
    setIsCustomKey(false);
    showAlert('Security', 'Switched to random encryption key. Cache re-encrypted.');
  }

  async function clearAllSettings() {
    await keyring.clearAllCredentials();
    setCaldavServer('');
    setCaldavUsername('');
    setCaldavPassword('');
    setNcServer('');
    setGiteaInstance('');
    setGiteaToken('');
    setGiteaRepos('');
    setCaldavConfigured(false);
    setGiteaConfigured(false);
    setCaldavStatus('unknown');
    setGiteaStatus('unknown');
    setAuthMethod('manual');
    setAvailableCalendars([]);
    setCalendarSelection(new Set());
    setSelectedCalendars([]);
    setDefaultEventCalendar('');
    setDefaultTaskCalendar('');
    showAlert('Cleared', 'All credentials have been removed');
  }

  async function clearCachedData() {
    await cache.clearCache();
    setLastSync(null);
    showAlert('Cleared', 'Cached data has been removed');
  }

  function showAlert(title: string, message: string) {
    if (Platform.OS === 'web') {
      window.alert(`${title}: ${message}`);
    } else {
      Alert.alert(title, message);
    }
  }

  function getStatusColor(status: string): string {
    switch (status) {
      case 'success': return '#4CAF50';
      case 'error': return '#F44336';
      case 'testing': return '#FF9800';
      default: return '#9E9E9E';
    }
  }

  const themeOptions: { value: ThemePreference; label: string }[] = [
    { value: 'system', label: 'System' },
    { value: 'light', label: 'Light' },
    { value: 'dark', label: 'Dark' },
  ];

  const c = theme.colors;

  return (
    <ScrollView style={[styles.container, { backgroundColor: c.background }]}>
      {/* Appearance */}
      <View style={[styles.section, { backgroundColor: c.surface }]}>
        <Text style={[styles.sectionTitle, { color: c.text }]}>Appearance</Text>
        <Text style={[styles.label, { color: c.text }]}>Theme</Text>
        <View style={styles.themeRow}>
          {themeOptions.map((opt) => (
            <TouchableOpacity
              key={opt.value}
              style={[
                styles.themeButton,
                { backgroundColor: c.filterChip },
                state.themePreference === opt.value && { backgroundColor: c.primary },
              ]}
              onPress={() => setThemePreference(opt.value)}
            >
              <Text
                style={[
                  styles.themeButtonText,
                  { color: state.themePreference === opt.value ? '#fff' : c.textSecondary },
                ]}
              >
                {opt.label}
              </Text>
            </TouchableOpacity>
          ))}
        </View>
      </View>

      {/* Task Input Defaults */}
      <View style={[styles.section, { backgroundColor: c.surface }]}>
        <Text style={[styles.sectionTitle, { color: c.text }]}>Task Input</Text>
        <Text style={[styles.hint, { color: c.textSecondary, marginBottom: 12 }]}>
          Configure default hours for time keywords like "tonight" or "tomorrow morning"
        </Text>

        <View style={styles.taskDefaultsGrid}>
          <View style={styles.taskDefaultRow}>
            <Text style={[styles.label, { color: c.text, flex: 1 }]}>Morning hour</Text>
            <TextInput
              style={[styles.input, styles.taskDefaultInput, { borderColor: c.border, backgroundColor: c.inputBackground, color: c.text }]}
              value={taskMorning}
              onChangeText={setTaskMorning}
              keyboardType="numeric"
              placeholder="8"
              placeholderTextColor={c.textTertiary}
            />
          </View>
          <View style={styles.taskDefaultRow}>
            <Text style={[styles.label, { color: c.text, flex: 1 }]}>Afternoon hour</Text>
            <TextInput
              style={[styles.input, styles.taskDefaultInput, { borderColor: c.border, backgroundColor: c.inputBackground, color: c.text }]}
              value={taskAfternoon}
              onChangeText={setTaskAfternoon}
              keyboardType="numeric"
              placeholder="14"
              placeholderTextColor={c.textTertiary}
            />
          </View>
          <View style={styles.taskDefaultRow}>
            <Text style={[styles.label, { color: c.text, flex: 1 }]}>Night hour</Text>
            <TextInput
              style={[styles.input, styles.taskDefaultInput, { borderColor: c.border, backgroundColor: c.inputBackground, color: c.text }]}
              value={taskNight}
              onChangeText={setTaskNight}
              keyboardType="numeric"
              placeholder="21"
              placeholderTextColor={c.textTertiary}
            />
          </View>
          <View style={styles.taskDefaultRow}>
            <Text style={[styles.label, { color: c.text, flex: 1 }]}>Default hour</Text>
            <TextInput
              style={[styles.input, styles.taskDefaultInput, { borderColor: c.border, backgroundColor: c.inputBackground, color: c.text }]}
              value={taskDefault}
              onChangeText={setTaskDefault}
              keyboardType="numeric"
              placeholder="10"
              placeholderTextColor={c.textTertiary}
            />
          </View>
        </View>

        <TouchableOpacity style={[styles.button, { backgroundColor: c.primary }]} onPress={saveTaskDefaultsHandler}>
          <Text style={styles.buttonText}>Save Defaults</Text>
        </TouchableOpacity>
      </View>

      {/* Pomodoro Timer */}
      <View style={[styles.section, { backgroundColor: c.surface }]}>
        <Text style={[styles.sectionTitle, { color: c.text }]}>Pomodoro Timer</Text>
        <Text style={[styles.hint, { color: c.textSecondary, marginBottom: 12 }]}>
          Configure work and break durations for the Pomodoro timer
        </Text>

        <View style={styles.taskDefaultsGrid}>
          <View style={styles.taskDefaultRow}>
            <Text style={[styles.label, { color: c.text, flex: 1 }]}>Work (min)</Text>
            <TextInput
              style={[styles.input, styles.taskDefaultInput, { borderColor: c.border, backgroundColor: c.inputBackground, color: c.text }]}
              value={pomWork}
              onChangeText={setPomWork}
              keyboardType="numeric"
              placeholder="25"
              placeholderTextColor={c.textTertiary}
            />
          </View>
          <View style={styles.taskDefaultRow}>
            <Text style={[styles.label, { color: c.text, flex: 1 }]}>Short break (min)</Text>
            <TextInput
              style={[styles.input, styles.taskDefaultInput, { borderColor: c.border, backgroundColor: c.inputBackground, color: c.text }]}
              value={pomShortBreak}
              onChangeText={setPomShortBreak}
              keyboardType="numeric"
              placeholder="5"
              placeholderTextColor={c.textTertiary}
            />
          </View>
          <View style={styles.taskDefaultRow}>
            <Text style={[styles.label, { color: c.text, flex: 1 }]}>Long break (min)</Text>
            <TextInput
              style={[styles.input, styles.taskDefaultInput, { borderColor: c.border, backgroundColor: c.inputBackground, color: c.text }]}
              value={pomLongBreak}
              onChangeText={setPomLongBreak}
              keyboardType="numeric"
              placeholder="15"
              placeholderTextColor={c.textTertiary}
            />
          </View>
          <View style={styles.taskDefaultRow}>
            <Text style={[styles.label, { color: c.text, flex: 1 }]}>Sessions until long break</Text>
            <TextInput
              style={[styles.input, styles.taskDefaultInput, { borderColor: c.border, backgroundColor: c.inputBackground, color: c.text }]}
              value={pomSessions}
              onChangeText={setPomSessions}
              keyboardType="numeric"
              placeholder="4"
              placeholderTextColor={c.textTertiary}
            />
          </View>
        </View>

        <TouchableOpacity style={[styles.button, { backgroundColor: c.primary }]} onPress={savePomodoroHandler}>
          <Text style={styles.buttonText}>Save Pomodoro Settings</Text>
        </TouchableOpacity>
      </View>

      {/* Security */}
      <View style={[styles.section, { backgroundColor: c.surface }]}>
        <Text style={[styles.sectionTitle, { color: c.text }]}>Security</Text>
        <Text style={[styles.hint, { color: c.textSecondary, marginBottom: 12 }]}>
          Cache encryption is active ({isCustomKey ? 'custom passphrase' : 'random key'})
        </Text>

        <Text style={[styles.label, { color: c.text }]}>Set Custom Passphrase</Text>
        <TextInput
          style={[styles.input, { borderColor: c.border, backgroundColor: c.inputBackground, color: c.text }]}
          value={passphrase}
          onChangeText={setPassphrase}
          placeholder="Min 8 characters"
          placeholderTextColor={c.textTertiary}
          secureTextEntry
          autoCapitalize="none"
          editable={!securityBusy}
        />
        <TouchableOpacity
          style={[styles.button, { backgroundColor: c.primary, opacity: securityBusy ? 0.5 : 1 }]}
          onPress={handleSetCustomPassphrase}
          disabled={securityBusy}
        >
          <Text style={styles.buttonText}>
            {securityBusy ? 'Re-encrypting...' : 'Set Custom Passphrase'}
          </Text>
        </TouchableOpacity>

        {isCustomKey && (
          <TouchableOpacity
            style={[styles.button, styles.secondaryButton, { backgroundColor: c.filterChip, opacity: securityBusy ? 0.5 : 1 }]}
            onPress={handleResetToRandomKey}
            disabled={securityBusy}
          >
            <Text style={[styles.buttonText, { color: c.text }]}>Reset to Random Key</Text>
          </TouchableOpacity>
        )}
      </View>

      {/* Notifications */}
      {Platform.OS !== 'web' && (
        <View style={[styles.section, { backgroundColor: c.surface }]}>
          <Text style={[styles.sectionTitle, { color: c.text }]}>Notifications</Text>

          <TouchableOpacity
            style={[
              styles.toggleRow,
              { backgroundColor: notifEnabled ? c.primary : c.filterChip },
            ]}
            onPress={toggleNotifications}
          >
            <Text style={[styles.toggleText, { color: notifEnabled ? '#fff' : c.textSecondary }]}>
              Event Reminders: {notifEnabled ? 'ON' : 'OFF'}
            </Text>
          </TouchableOpacity>

          {notifEnabled && (
            <>
              <Text style={[styles.label, { color: c.text, marginTop: 12 }]}>Remind before (minutes)</Text>
              <View style={styles.reminderRow}>
                <TextInput
                  style={[styles.input, styles.reminderInput, { borderColor: c.border, backgroundColor: c.inputBackground, color: c.text }]}
                  value={reminderMinutes}
                  onChangeText={setReminderMinutes}
                  keyboardType="numeric"
                  placeholder="15"
                  placeholderTextColor={c.textTertiary}
                />
                <TouchableOpacity
                  style={[styles.smallButton, { backgroundColor: c.primary }]}
                  onPress={saveReminderMinutes}
                >
                  <Text style={styles.smallButtonText}>Save</Text>
                </TouchableOpacity>
              </View>
            </>
          )}

          {/* UnifiedPush section */}
          {upAvailable && (
            <>
              <View style={[styles.divider, { backgroundColor: c.border }]} />
              <Text style={[styles.subsectionTitle, { color: c.text }]}>UnifiedPush</Text>
              <Text style={[styles.hint, { color: c.textTertiary }]}>
                Receive push notifications from Gitea and other services via your installed UP distributor.
              </Text>

              {upDistributors.length === 0 ? (
                <Text style={[styles.hint, { color: c.textTertiary, marginTop: 8 }]}>
                  No UP distributors found. Install ntfy or NextPush.
                </Text>
              ) : (
                <>
                  <Text style={[styles.label, { color: c.text, marginTop: 8 }]}>Distributor</Text>
                  <View style={styles.themeRow}>
                    {upDistributors.map((dist) => {
                      const shortName = dist.split('.').pop() || dist;
                      return (
                        <TouchableOpacity
                          key={dist}
                          style={[
                            styles.themeButton,
                            { backgroundColor: c.filterChip },
                            upDistributor === dist && { backgroundColor: c.primary },
                          ]}
                          onPress={() => selectDistributor(dist)}
                        >
                          <Text
                            style={[
                              styles.themeButtonText,
                              { color: upDistributor === dist ? '#fff' : c.textSecondary },
                            ]}
                          >
                            {shortName}
                          </Text>
                        </TouchableOpacity>
                      );
                    })}
                  </View>

                  {!upRegistered ? (
                    <TouchableOpacity
                      style={[styles.button, { backgroundColor: c.primary }]}
                      onPress={registerUP}
                    >
                      <Text style={styles.buttonText}>Register with UnifiedPush</Text>
                    </TouchableOpacity>
                  ) : (
                    <>
                      {upEndpoint && (
                        <View style={[styles.endpointBox, { backgroundColor: c.inputBackground, borderColor: c.border }]}>
                          <Text style={[styles.label, { color: c.text, marginBottom: 4 }]}>Endpoint URL</Text>
                          <Text style={[styles.endpointText, { color: c.textSecondary }]} selectable>
                            {upEndpoint}
                          </Text>
                          <Text style={[styles.hint, { color: c.textTertiary, marginTop: 4 }]}>
                            Use this URL as a webhook target in Gitea or other services.
                          </Text>
                        </View>
                      )}
                      <TouchableOpacity
                        style={[styles.button, styles.dangerButton]}
                        onPress={unregisterUP}
                      >
                        <Text style={styles.buttonText}>Unregister</Text>
                      </TouchableOpacity>
                    </>
                  )}
                </>
              )}
            </>
          )}
        </View>
      )}

      {/* CalDAV */}
      <View style={[styles.section, { backgroundColor: c.surface }]}>
        <View style={styles.sectionHeader}>
          <Text style={[styles.sectionTitle, { color: c.text }]}>CalDAV Settings</Text>
          <View style={[styles.statusDot, { backgroundColor: getStatusColor(caldavStatus) }]} />
        </View>

        <Text style={[styles.label, { color: c.text }]}>Login Method</Text>
        <View style={styles.themeRow}>
          {([{ value: 'manual' as AuthMethod, label: 'Manual' }, { value: 'nextcloud' as AuthMethod, label: 'Nextcloud' }]).map((opt) => (
            <TouchableOpacity
              key={opt.value}
              style={[
                styles.themeButton,
                { backgroundColor: c.filterChip },
                authMethod === opt.value && { backgroundColor: c.primary },
              ]}
              onPress={() => setAuthMethod(opt.value)}
            >
              <Text
                style={[
                  styles.themeButtonText,
                  { color: authMethod === opt.value ? '#fff' : c.textSecondary },
                ]}
              >
                {opt.label}
              </Text>
            </TouchableOpacity>
          ))}
        </View>

        {authMethod === 'nextcloud' ? (
          <>
            <Text style={[styles.label, { color: c.text, marginTop: 12 }]}>Nextcloud Server URL</Text>
            <TextInput
              style={[styles.input, { borderColor: c.border, backgroundColor: c.inputBackground, color: c.text }]}
              value={ncServer}
              onChangeText={setNcServer}
              placeholder="https://cloud.example.com"
              placeholderTextColor={c.textTertiary}
              autoCapitalize="none"
              autoCorrect={false}
              editable={!ncLogging}
            />

            {Platform.OS === 'web' && (
              <Text style={[styles.hint, { color: '#FF9800', marginBottom: 8 }]}>
                Nextcloud Login Flow is not available on web (CORS). Use manual entry with an app password instead.
              </Text>
            )}

            {ncLogging ? (
              <View style={styles.ncPollingRow}>
                <ActivityIndicator size="small" color={c.primary} />
                <Text style={[styles.hint, { color: c.textSecondary, marginLeft: 8, flex: 1 }]}>
                  Waiting for login in browser...
                </Text>
                <TouchableOpacity onPress={cancelNextcloudLogin}>
                  <Text style={{ color: '#F44336', fontWeight: '600' }}>Cancel</Text>
                </TouchableOpacity>
              </View>
            ) : (
              <TouchableOpacity
                style={[styles.button, { backgroundColor: '#0082C9' }]}
                onPress={startNextcloudLogin}
              >
                <Text style={styles.buttonText}>Login with Nextcloud</Text>
              </TouchableOpacity>
            )}

            {caldavServer !== '' && (
              <>
                <View style={[styles.divider, { backgroundColor: c.border }]} />
                <Text style={[styles.hint, { color: c.textSecondary, marginBottom: 4 }]}>
                  Logged in as: {caldavUsername}
                </Text>
                <Text style={[styles.hint, { color: c.textTertiary, marginBottom: 8, fontSize: 12 }]}>
                  {caldavServer}
                </Text>
              </>
            )}
          </>
        ) : (
          <>
            <Text style={[styles.label, { color: c.text, marginTop: 12 }]}>Server URL</Text>
            <TextInput
              style={[styles.input, { borderColor: c.border, backgroundColor: c.inputBackground, color: c.text }]}
              value={caldavServer}
              onChangeText={setCaldavServer}
              placeholder="https://caldav.example.com/dav"
              placeholderTextColor={c.textTertiary}
              autoCapitalize="none"
              autoCorrect={false}
            />

            <Text style={[styles.label, { color: c.text }]}>Username</Text>
            <TextInput
              style={[styles.input, { borderColor: c.border, backgroundColor: c.inputBackground, color: c.text }]}
              value={caldavUsername}
              onChangeText={setCaldavUsername}
              placeholder="username"
              placeholderTextColor={c.textTertiary}
              autoCapitalize="none"
              autoCorrect={false}
            />

            <Text style={[styles.label, { color: c.text }]}>Password</Text>
            <TextInput
              style={[styles.input, { borderColor: c.border, backgroundColor: c.inputBackground, color: c.text }]}
              value={caldavPassword}
              onChangeText={setCaldavPassword}
              placeholder="••••••••"
              placeholderTextColor={c.textTertiary}
              secureTextEntry
              autoCapitalize="none"
            />

            <TouchableOpacity style={[styles.button, { backgroundColor: c.primary }]} onPress={saveCalDav}>
              <Text style={styles.buttonText}>Save & Test CalDAV</Text>
            </TouchableOpacity>
          </>
        )}
      </View>

      {/* Calendar Picker */}
      {caldavStatus === 'success' && (
        <View style={[styles.section, { backgroundColor: c.surface }]}>
          <Text style={[styles.sectionTitle, { color: c.text }]}>Calendars</Text>

          {fetchingCalendars ? (
            <ActivityIndicator size="small" color={c.primary} style={{ marginVertical: 12 }} />
          ) : availableCalendars.length > 0 ? (
            <>
              <View style={[styles.calPickerActions, { marginBottom: 8 }]}>
                <TouchableOpacity onPress={selectAllCalendars}>
                  <Text style={{ color: c.primary, fontWeight: '600', fontSize: 13 }}>Select All</Text>
                </TouchableOpacity>
                <TouchableOpacity onPress={deselectAllCalendars} style={{ marginLeft: 16 }}>
                  <Text style={{ color: c.primary, fontWeight: '600', fontSize: 13 }}>Deselect All</Text>
                </TouchableOpacity>
              </View>

              {availableCalendars.map((cal) => (
                <TouchableOpacity
                  key={cal.href}
                  style={[styles.calendarRow, { borderColor: c.border }]}
                  onPress={() => toggleCalendarSelection(cal.href)}
                >
                  <View
                    style={[
                      styles.calCheckbox,
                      { borderColor: c.border },
                      calendarSelection.has(cal.href) && { backgroundColor: c.primary, borderColor: c.primary },
                    ]}
                  >
                    {calendarSelection.has(cal.href) && (
                      <Text style={{ color: '#fff', fontSize: 12, fontWeight: '700' }}>✓</Text>
                    )}
                  </View>

                  {cal.color && (
                    <View style={[styles.calColorDot, { backgroundColor: cal.color }]} />
                  )}

                  <Text style={[styles.calName, { color: c.text }]} numberOfLines={1}>
                    {cal.displayName}
                  </Text>

                  <View style={styles.calBadges}>
                    {cal.components.map((comp) => (
                      <View key={comp} style={[styles.calBadge, { backgroundColor: c.filterChip }]}>
                        <Text style={[styles.calBadgeText, { color: c.textSecondary }]}>
                          {comp === 'VEVENT' ? 'Events' : comp === 'VTODO' ? 'Tasks' : comp === 'VJOURNAL' ? 'Notes' : comp}
                        </Text>
                      </View>
                    ))}
                  </View>
                </TouchableOpacity>
              ))}

              {calendarSelection.size === 0 && (
                <Text style={[styles.hint, { color: '#FF9800', marginTop: 4 }]}>
                  No calendars selected. Select at least one to sync.
                </Text>
              )}

              <TouchableOpacity style={[styles.button, { backgroundColor: c.primary }]} onPress={saveCalendarSelection}>
                <Text style={styles.buttonText}>Save Calendar Selection</Text>
              </TouchableOpacity>

              {/* Default calendar pickers */}
              <View style={[styles.divider, { backgroundColor: c.border }]} />
              <Text style={[styles.subsectionTitle, { color: c.text }]}>Default Calendars</Text>
              <Text style={[styles.hint, { color: c.textTertiary, marginBottom: 8 }]}>
                New events and tasks will be saved to these calendars.
              </Text>

              <Text style={[styles.label, { color: c.text }]}>Default Event Calendar</Text>
              <View style={styles.defaultCalPicker}>
                {availableCalendars
                  .filter((cal) => calendarSelection.has(cal.href) && cal.components.includes('VEVENT'))
                  .map((cal) => (
                    <TouchableOpacity
                      key={cal.href}
                      style={[
                        styles.defaultCalOption,
                        { borderColor: c.border },
                        defaultEventCalendar === cal.href && { backgroundColor: c.primary, borderColor: c.primary },
                      ]}
                      onPress={() => setDefaultEventCal(cal.href)}
                    >
                      {cal.color && <View style={[styles.defaultCalDot, { backgroundColor: cal.color }]} />}
                      <Text
                        style={[
                          styles.defaultCalText,
                          { color: defaultEventCalendar === cal.href ? '#fff' : c.text },
                        ]}
                        numberOfLines={1}
                      >
                        {cal.displayName}
                      </Text>
                    </TouchableOpacity>
                  ))}
              </View>

              <Text style={[styles.label, { color: c.text }]}>Default Task Calendar</Text>
              <View style={styles.defaultCalPicker}>
                {availableCalendars
                  .filter((cal) => calendarSelection.has(cal.href) && cal.components.includes('VTODO'))
                  .map((cal) => (
                    <TouchableOpacity
                      key={cal.href}
                      style={[
                        styles.defaultCalOption,
                        { borderColor: c.border },
                        defaultTaskCalendar === cal.href && { backgroundColor: c.primary, borderColor: c.primary },
                      ]}
                      onPress={() => setDefaultTaskCal(cal.href)}
                    >
                      {cal.color && <View style={[styles.defaultCalDot, { backgroundColor: cal.color }]} />}
                      <Text
                        style={[
                          styles.defaultCalText,
                          { color: defaultTaskCalendar === cal.href ? '#fff' : c.text },
                        ]}
                        numberOfLines={1}
                      >
                        {cal.displayName}
                      </Text>
                    </TouchableOpacity>
                  ))}
              </View>
            </>
          ) : (
            <>
              <Text style={[styles.hint, { color: c.textTertiary }]}>No calendars found.</Text>
              <TouchableOpacity style={[styles.button, styles.secondaryButton, { backgroundColor: c.filterChip }]} onPress={loadCalendars}>
                <Text style={[styles.buttonText, { color: c.text }]}>Refresh Calendars</Text>
              </TouchableOpacity>
            </>
          )}
        </View>
      )}

      {/* Gitea */}
      <View style={[styles.section, { backgroundColor: c.surface }]}>
        <View style={styles.sectionHeader}>
          <Text style={[styles.sectionTitle, { color: c.text }]}>Gitea Settings</Text>
          <View style={[styles.statusDot, { backgroundColor: getStatusColor(giteaStatus) }]} />
        </View>

        <Text style={[styles.label, { color: c.text }]}>Instance URL</Text>
        <TextInput
          style={[styles.input, { borderColor: c.border, backgroundColor: c.inputBackground, color: c.text }]}
          value={giteaInstance}
          onChangeText={setGiteaInstance}
          placeholder="https://gitea.example.com"
          placeholderTextColor={c.textTertiary}
          autoCapitalize="none"
          autoCorrect={false}
        />

        <Text style={[styles.label, { color: c.text }]}>Personal Access Token</Text>
        <TextInput
          style={[styles.input, { borderColor: c.border, backgroundColor: c.inputBackground, color: c.text }]}
          value={giteaToken}
          onChangeText={setGiteaToken}
          placeholder="your-access-token"
          placeholderTextColor={c.textTertiary}
          secureTextEntry
          autoCapitalize="none"
        />

        <Text style={[styles.label, { color: c.text }]}>Repositories (one per line, owner/repo format)</Text>
        <TextInput
          style={[styles.input, styles.multiline, { borderColor: c.border, backgroundColor: c.inputBackground, color: c.text }]}
          value={giteaRepos}
          onChangeText={setGiteaRepos}
          placeholder={'owner/repo1\nowner/repo2'}
          placeholderTextColor={c.textTertiary}
          multiline
          numberOfLines={4}
          autoCapitalize="none"
          autoCorrect={false}
        />

        <TouchableOpacity style={[styles.button, { backgroundColor: c.primary }]} onPress={saveGitea}>
          <Text style={styles.buttonText}>Save & Test Gitea</Text>
        </TouchableOpacity>
      </View>

      {/* Data */}
      <View style={[styles.section, { backgroundColor: c.surface }]}>
        <Text style={[styles.sectionTitle, { color: c.text }]}>Data</Text>
        {state.lastSync && (
          <Text style={[styles.lastSyncText, { color: c.textSecondary }]}>
            Last synced: {state.lastSync.toLocaleString()}
          </Text>
        )}
        <TouchableOpacity style={[styles.button, styles.secondaryButton, { backgroundColor: c.filterChip }]} onPress={clearCachedData}>
          <Text style={[styles.buttonText, { color: c.text }]}>Clear Cached Data</Text>
        </TouchableOpacity>
        <TouchableOpacity style={[styles.button, styles.dangerButton]} onPress={clearAllSettings}>
          <Text style={styles.buttonText}>Clear All Credentials</Text>
        </TouchableOpacity>
      </View>
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
  },
  section: {
    margin: 16,
    padding: 16,
    borderRadius: 8,
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 1 },
    shadowOpacity: 0.1,
    shadowRadius: 2,
    elevation: 2,
  },
  sectionHeader: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    marginBottom: 16,
  },
  sectionTitle: {
    fontSize: 18,
    fontWeight: '600',
    marginBottom: 12,
  },
  subsectionTitle: {
    fontSize: 16,
    fontWeight: '600',
    marginBottom: 4,
  },
  statusDot: {
    width: 12,
    height: 12,
    borderRadius: 6,
  },
  label: {
    fontSize: 14,
    fontWeight: '500',
    marginBottom: 4,
  },
  hint: {
    fontSize: 13,
  },
  input: {
    borderWidth: 1,
    borderRadius: 8,
    padding: 12,
    marginBottom: 12,
    fontSize: 16,
  },
  multiline: {
    minHeight: 100,
    textAlignVertical: 'top',
  },
  themeRow: {
    flexDirection: 'row',
    gap: 8,
    marginBottom: 4,
  },
  themeButton: {
    flex: 1,
    paddingVertical: 10,
    borderRadius: 8,
    alignItems: 'center',
  },
  themeButtonText: {
    fontSize: 14,
    fontWeight: '500',
  },
  toggleRow: {
    padding: 14,
    borderRadius: 8,
    alignItems: 'center',
  },
  toggleText: {
    fontSize: 16,
    fontWeight: '600',
  },
  reminderRow: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 8,
  },
  reminderInput: {
    flex: 1,
  },
  smallButton: {
    paddingHorizontal: 16,
    paddingVertical: 12,
    borderRadius: 8,
  },
  smallButtonText: {
    color: '#fff',
    fontSize: 14,
    fontWeight: '600',
  },
  divider: {
    height: 1,
    marginVertical: 16,
  },
  endpointBox: {
    padding: 12,
    borderRadius: 8,
    borderWidth: 1,
    marginTop: 8,
  },
  endpointText: {
    fontSize: 12,
    fontFamily: Platform.OS === 'web' ? 'monospace' : undefined,
  },
  button: {
    padding: 14,
    borderRadius: 8,
    alignItems: 'center',
    marginTop: 8,
  },
  secondaryButton: {},
  dangerButton: {
    backgroundColor: '#F44336',
  },
  buttonText: {
    color: '#fff',
    fontSize: 16,
    fontWeight: '600',
  },
  lastSyncText: {
    fontSize: 13,
    marginBottom: 8,
  },
  taskDefaultsGrid: {
    gap: 4,
    marginBottom: 8,
  },
  taskDefaultRow: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 12,
  },
  taskDefaultInput: {
    width: 64,
    textAlign: 'center',
    flex: 0,
  },
  ncPollingRow: {
    flexDirection: 'row',
    alignItems: 'center',
    paddingVertical: 12,
  },
  calPickerActions: {
    flexDirection: 'row',
    alignItems: 'center',
  },
  calendarRow: {
    flexDirection: 'row',
    alignItems: 'center',
    paddingVertical: 10,
    borderBottomWidth: 1,
    gap: 8,
  },
  calCheckbox: {
    width: 22,
    height: 22,
    borderRadius: 4,
    borderWidth: 2,
    alignItems: 'center',
    justifyContent: 'center',
  },
  calColorDot: {
    width: 14,
    height: 14,
    borderRadius: 7,
  },
  calName: {
    flex: 1,
    fontSize: 15,
    fontWeight: '500',
  },
  calBadges: {
    flexDirection: 'row',
    gap: 4,
  },
  calBadge: {
    paddingHorizontal: 6,
    paddingVertical: 2,
    borderRadius: 8,
  },
  calBadgeText: {
    fontSize: 10,
    fontWeight: '600',
  },
  defaultCalPicker: {
    flexDirection: 'row',
    flexWrap: 'wrap',
    gap: 6,
    marginBottom: 12,
  },
  defaultCalOption: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 6,
    borderWidth: 1,
    borderRadius: 8,
    paddingHorizontal: 10,
    paddingVertical: 8,
    maxWidth: 180,
  },
  defaultCalDot: {
    width: 10,
    height: 10,
    borderRadius: 5,
  },
  defaultCalText: {
    fontSize: 13,
    fontWeight: '500',
  },
});
