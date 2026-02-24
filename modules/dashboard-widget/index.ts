import { Platform } from 'react-native';
import { requireNativeModule } from 'expo-modules-core';

interface WidgetNativeModule {
  updateWidgetData(eventRowsStr: string, taskRowsStr: string, issuesCount: number, lastSync: string): boolean;
  updateWidgetStats(eventsRemainingToday: number, pomodoroSessionsToday: number, overdueTaskRowsStr: string): boolean;
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
  saveWorkerNotificationSettings(enabled: boolean, minutesBefore: number): boolean;
  canScheduleExactAlarms(): boolean;
  requestExactAlarmPermission(): boolean;
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

export function updateWidgetStats(
  eventsRemainingToday: number,
  pomodoroSessionsToday: number,
  overdueTaskRowsStr: string
): boolean {
  if (!nativeModule) return false;
  return nativeModule.updateWidgetStats(eventsRemainingToday, pomodoroSessionsToday, overdueTaskRowsStr);
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

export function saveWorkerNotificationSettings(enabled: boolean, minutesBefore: number): boolean {
  if (!nativeModule) return false;
  return nativeModule.saveWorkerNotificationSettings(enabled, minutesBefore);
}

export function canScheduleExactAlarms(): boolean {
  if (!nativeModule) return true;
  return nativeModule.canScheduleExactAlarms();
}

export function requestExactAlarmPermission(): boolean {
  if (!nativeModule) return false;
  return nativeModule.requestExactAlarmPermission();
}
