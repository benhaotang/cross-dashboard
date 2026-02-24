import { Platform } from 'react-native';
import { requireNativeModule } from 'expo-modules-core';

interface WidgetNativeModule {
  updateWidgetData(eventsCount: number, issuesCount: number, nextEvent: string, lastSync: string): boolean;
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
  eventsCount: number,
  issuesCount: number,
  nextEvent: string,
  lastSync: string
): boolean {
  if (!nativeModule) return false;
  return nativeModule.updateWidgetData(eventsCount, issuesCount, nextEvent, lastSync);
}

export function forceRefresh(): boolean {
  if (!nativeModule) return false;
  return nativeModule.forceRefresh();
}
