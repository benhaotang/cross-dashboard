import * as SecureStore from 'expo-secure-store';
import { Platform } from 'react-native';

const KEYRING_PREFIX = 'cross_dashboard_';

export type CredentialKey =
  | 'caldav_password'
  | 'caldav_server'
  | 'caldav_username'
  | 'caldav_auth_method'
  | 'caldav_selected_calendars'
  | 'gitea_token'
  | 'gitea_instance'
  | 'notif_enabled'
  | 'notif_minutes'
  | 'up_endpoint'
  | 'encryption_key'
  | 'encryption_key_custom';

function getFullKey(key: CredentialKey): string {
  return `${KEYRING_PREFIX}${key}`;
}

export async function setCredential(key: CredentialKey, value: string): Promise<void> {
  const fullKey = getFullKey(key);
  
  if (Platform.OS === 'web') {
    // Web fallback - using localStorage (less secure, consider encryption)
    localStorage.setItem(fullKey, value);
    return;
  }
  
  await SecureStore.setItemAsync(fullKey, value, {
    keychainAccessible: SecureStore.WHEN_UNLOCKED,
  });
}

export async function getCredential(key: CredentialKey): Promise<string | null> {
  const fullKey = getFullKey(key);
  
  if (Platform.OS === 'web') {
    return localStorage.getItem(fullKey);
  }
  
  return await SecureStore.getItemAsync(fullKey);
}

export async function deleteCredential(key: CredentialKey): Promise<void> {
  const fullKey = getFullKey(key);
  
  if (Platform.OS === 'web') {
    localStorage.removeItem(fullKey);
    return;
  }
  
  await SecureStore.deleteItemAsync(fullKey);
}

export async function hasCredential(key: CredentialKey): Promise<boolean> {
  const value = await getCredential(key);
  return value !== null;
}

export async function clearAllCredentials(): Promise<void> {
  const keys: CredentialKey[] = [
    'caldav_password',
    'caldav_server',
    'caldav_username',
    'caldav_auth_method',
    'caldav_selected_calendars',
    'gitea_token',
    'gitea_instance',
  ];
  
  await Promise.all(keys.map(key => deleteCredential(key)));
}
