import { Platform } from 'react-native';
import { requireNativeModule } from 'expo-modules-core';

interface WidgetNativeModule {
  updateWidgetData(eventRowsStr: string, taskRowsStr: string, issuesCount: number, lastSync: string): boolean;
  saveWorkerCredentials(
    caldavServer: string,
    caldavUser: string,
    caldavPass: string,
    calendarHrefs: string,
    giteaUrl: string,
    giteaToken: string,
    giteaRepos: string
  ): boolean;
  scheduleSync(intervalMinutes: number): boolean;
  cancelSync(): boolean;
  forceRefresh(): boolean;
}

let nativeModule: WidgetNativeModule | null = null;
if (Platform.OS === 'android') {
  try {
    nativeModule = requireNativeModule<WidgetNativeModule>('DashboardWidget');
  } catch {
    // Module not available (e.g. in Expo Go)
  }
}

export function isAvailable(): boolean {
  return nativeModule !== null;
}

export function updateWidgetData(
  eventRowsStr: string,
  taskRowsStr: string,
  issuesCount: number,
  lastSync: string
): boolean {
  if (!nativeModule) return false;
  return nativeModule.updateWidgetData(eventRowsStr, taskRowsStr, issuesCount, lastSync);
}

export function saveWorkerCredentials(
  caldavServer: string,
  caldavUser: string,
  caldavPass: string,
  calendarHrefs: string,
  giteaUrl: string,
  giteaToken: string,
  giteaRepos: string
): boolean {
  if (!nativeModule) return false;
  return nativeModule.saveWorkerCredentials(
    caldavServer, caldavUser, caldavPass, calendarHrefs, giteaUrl, giteaToken, giteaRepos
  );
}

export function scheduleSync(intervalMinutes: number): boolean {
  if (!nativeModule) return false;
  return nativeModule.scheduleSync(intervalMinutes);
}

export function cancelSync(): boolean {
  if (!nativeModule) return false;
  return nativeModule.cancelSync();
}

export function forceRefresh(): boolean {
  if (!nativeModule) return false;
  return nativeModule.forceRefresh();
}
