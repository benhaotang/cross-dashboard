import React, { useState, useEffect } from 'react';
import {
  View,
  Text,
  StyleSheet,
  TextInput,
  TouchableOpacity,
  ScrollView,
  Alert,
  Platform,
} from 'react-native';
import { useApp } from '../store/AppContext';
import { useTheme } from '../hooks/useTheme';
import { ThemePreference } from '../theme';
import * as keyring from '../services/keyring';
import * as caldav from '../services/caldav';
import * as gitea from '../services/gitea';
import * as cache from '../services/cache';
import * as notifications from '../services/notifications';

export default function SettingsScreen() {
  const { setCaldavConfigured, setGiteaConfigured, setGiteaRepositories, setThemePreference, setLastSync, state } =
    useApp();
  const theme = useTheme();

  // CalDAV settings
  const [caldavServer, setCaldavServer] = useState('');
  const [caldavUsername, setCaldavUsername] = useState('');
  const [caldavPassword, setCaldavPassword] = useState('');

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

  // Status
  const [caldavStatus, setCaldavStatus] = useState<'unknown' | 'testing' | 'success' | 'error'>('unknown');
  const [giteaStatus, setGiteaStatus] = useState<'unknown' | 'testing' | 'success' | 'error'>('unknown');

  useEffect(() => {
    loadSettings();
    loadNotificationSettings();
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
    const [server, username, instance] = await Promise.all([
      keyring.getCredential('caldav_server'),
      keyring.getCredential('caldav_username'),
      keyring.getCredential('gitea_instance'),
    ]);

    if (server) setCaldavServer(server);
    if (username) setCaldavUsername(username);
    if (instance) setGiteaInstance(instance);
    if (state.giteaRepositories.length > 0) setGiteaRepos(state.giteaRepositories.join('\n'));
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
    ]);

    setCaldavStatus('testing');
    const result = await caldav.testConnection();

    if (result.success) {
      setCaldavStatus('success');
      setCaldavConfigured(true);
      showAlert('Success', 'CalDAV connection successful!');
    } else {
      setCaldavStatus('error');
      showAlert('Error', `CalDAV connection failed: ${result.error}`);
    }
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

  async function clearAllSettings() {
    await keyring.clearAllCredentials();
    setCaldavServer('');
    setCaldavUsername('');
    setCaldavPassword('');
    setGiteaInstance('');
    setGiteaToken('');
    setGiteaRepos('');
    setCaldavConfigured(false);
    setGiteaConfigured(false);
    setCaldavStatus('unknown');
    setGiteaStatus('unknown');
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

        <Text style={[styles.label, { color: c.text }]}>Server URL</Text>
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
      </View>

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
});
