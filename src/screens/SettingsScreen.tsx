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
import * as keyring from '../services/keyring';
import * as caldav from '../services/caldav';
import * as gitea from '../services/gitea';

export default function SettingsScreen() {
  const { setCaldavConfigured, setGiteaConfigured, setGiteaRepositories, state } = useApp();

  // CalDAV settings
  const [caldavServer, setCaldavServer] = useState('');
  const [caldavUsername, setCaldavUsername] = useState('');
  const [caldavPassword, setCaldavPassword] = useState('');

  // Gitea settings
  const [giteaInstance, setGiteaInstance] = useState('');
  const [giteaToken, setGiteaToken] = useState('');
  const [giteaRepos, setGiteaRepos] = useState('');

  // Status
  const [caldavStatus, setCaldavStatus] = useState<'unknown' | 'testing' | 'success' | 'error'>(
    'unknown'
  );
  const [giteaStatus, setGiteaStatus] = useState<'unknown' | 'testing' | 'success' | 'error'>(
    'unknown'
  );

  useEffect(() => {
    loadSettings();
  }, []);

  async function loadSettings() {
    const [server, username, instance, repos] = await Promise.all([
      keyring.getCredential('caldav_server'),
      keyring.getCredential('caldav_username'),
      keyring.getCredential('gitea_instance'),
      keyring.getCredential('gitea_token').then(() => state.giteaRepositories.join('\n')),
    ]);

    if (server) setCaldavServer(server);
    if (username) setCaldavUsername(username);
    if (instance) setGiteaInstance(instance);
    if (repos) setGiteaRepos(repos);
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

  function showAlert(title: string, message: string) {
    if (Platform.OS === 'web') {
      window.alert(`${title}: ${message}`);
    } else {
      Alert.alert(title, message);
    }
  }

  function getStatusColor(status: string): string {
    switch (status) {
      case 'success':
        return '#4CAF50';
      case 'error':
        return '#F44336';
      case 'testing':
        return '#FF9800';
      default:
        return '#9E9E9E';
    }
  }

  return (
    <ScrollView style={styles.container}>
      <View style={styles.section}>
        <View style={styles.sectionHeader}>
          <Text style={styles.sectionTitle}>CalDAV Settings</Text>
          <View style={[styles.statusDot, { backgroundColor: getStatusColor(caldavStatus) }]} />
        </View>

        <Text style={styles.label}>Server URL</Text>
        <TextInput
          style={styles.input}
          value={caldavServer}
          onChangeText={setCaldavServer}
          placeholder="https://caldav.example.com/dav"
          autoCapitalize="none"
          autoCorrect={false}
        />

        <Text style={styles.label}>Username</Text>
        <TextInput
          style={styles.input}
          value={caldavUsername}
          onChangeText={setCaldavUsername}
          placeholder="username"
          autoCapitalize="none"
          autoCorrect={false}
        />

        <Text style={styles.label}>Password</Text>
        <TextInput
          style={styles.input}
          value={caldavPassword}
          onChangeText={setCaldavPassword}
          placeholder="********"
          secureTextEntry
          autoCapitalize="none"
        />

        <TouchableOpacity style={styles.button} onPress={saveCalDav}>
          <Text style={styles.buttonText}>Save & Test CalDAV</Text>
        </TouchableOpacity>
      </View>

      <View style={styles.section}>
        <View style={styles.sectionHeader}>
          <Text style={styles.sectionTitle}>Gitea Settings</Text>
          <View style={[styles.statusDot, { backgroundColor: getStatusColor(giteaStatus) }]} />
        </View>

        <Text style={styles.label}>Instance URL</Text>
        <TextInput
          style={styles.input}
          value={giteaInstance}
          onChangeText={setGiteaInstance}
          placeholder="https://gitea.example.com"
          autoCapitalize="none"
          autoCorrect={false}
        />

        <Text style={styles.label}>Personal Access Token</Text>
        <TextInput
          style={styles.input}
          value={giteaToken}
          onChangeText={setGiteaToken}
          placeholder="your-access-token"
          secureTextEntry
          autoCapitalize="none"
        />

        <Text style={styles.label}>Repositories (one per line, owner/repo format)</Text>
        <TextInput
          style={[styles.input, styles.multiline]}
          value={giteaRepos}
          onChangeText={setGiteaRepos}
          placeholder="owner/repo1&#10;owner/repo2"
          multiline
          numberOfLines={4}
          autoCapitalize="none"
          autoCorrect={false}
        />

        <TouchableOpacity style={styles.button} onPress={saveGitea}>
          <Text style={styles.buttonText}>Save & Test Gitea</Text>
        </TouchableOpacity>
      </View>

      <View style={styles.section}>
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
    backgroundColor: '#f5f5f5',
  },
  section: {
    backgroundColor: '#fff',
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
    color: '#333',
  },
  input: {
    borderWidth: 1,
    borderColor: '#ddd',
    borderRadius: 8,
    padding: 12,
    marginBottom: 12,
    fontSize: 16,
    backgroundColor: '#fafafa',
  },
  multiline: {
    minHeight: 100,
    textAlignVertical: 'top',
  },
  button: {
    backgroundColor: '#007AFF',
    padding: 14,
    borderRadius: 8,
    alignItems: 'center',
    marginTop: 8,
  },
  dangerButton: {
    backgroundColor: '#F44336',
  },
  buttonText: {
    color: '#fff',
    fontSize: 16,
    fontWeight: '600',
  },
});
