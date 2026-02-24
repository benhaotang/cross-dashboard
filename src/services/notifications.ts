import { Platform } from 'react-native';
import * as Notifications from 'expo-notifications';
import * as UnifiedPush from '../../modules/unified-push';
import { CalendarEvent } from '../types';
import { getCredential, setCredential, type CredentialKey } from './keyring';

// Configure notification handler
Notifications.setNotificationHandler({
  handleNotification: async () => ({
    shouldShowBanner: true,
    shouldShowList: true,
    shouldPlaySound: true,
    shouldSetBadge: false,
  }),
});

// --- Local notification scheduling for calendar events ---

export async function requestPermissions(): Promise<boolean> {
  if (Platform.OS === 'web' || Platform.OS === 'macos') return false;
  const { status } = await Notifications.requestPermissionsAsync();
  return status === 'granted';
}

export async function scheduleEventReminders(
  events: CalendarEvent[],
  minutesBefore: number = 15
): Promise<void> {
  if (Platform.OS === 'web' || Platform.OS === 'macos') return;
  // Cancel all existing event reminders first
  await cancelAllEventReminders();

  const now = new Date();
  const triggerOffset = minutesBefore * 60;

  for (const event of events) {
    const triggerTime = new Date(event.start.getTime() - triggerOffset * 1000);
    if (triggerTime <= now) continue;

    await Notifications.scheduleNotificationAsync({
      content: {
        title: event.summary,
        body: formatEventBody(event, minutesBefore),
        data: { type: 'event_reminder', uid: event.uid },
      },
      trigger: {
        type: Notifications.SchedulableTriggerInputTypes.DATE,
        date: triggerTime,
      },
    });
  }
}

function formatEventBody(event: CalendarEvent, minutesBefore: number): string {
  const timeStr = event.start.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' });
  let body = `Starts in ${minutesBefore} minutes at ${timeStr}`;
  if (event.location) body += ` - ${event.location}`;
  return body;
}

export async function cancelAllEventReminders(): Promise<void> {
  if (Platform.OS === 'web' || Platform.OS === 'macos') return;
  await Notifications.cancelAllScheduledNotificationsAsync();
}

// --- UnifiedPush integration ---

export function isUnifiedPushAvailable(): boolean {
  return UnifiedPush.isAvailable();
}

export function getUPDistributors(): string[] {
  return UnifiedPush.getDistributors();
}

export function getUPDistributor(): string {
  return UnifiedPush.getDistributor();
}

export function selectUPDistributor(distributor: string): boolean {
  return UnifiedPush.saveDistributor(distributor);
}

export function registerUnifiedPush(instance: string = 'default'): boolean {
  return UnifiedPush.register(instance);
}

export function unregisterUnifiedPush(instance: string = 'default'): boolean {
  return UnifiedPush.unregister(instance);
}

/**
 * Listen for a new UP endpoint. When received, store it and optionally
 * register it with the Gitea instance as a webhook.
 */
export function onUPEndpoint(
  callback: (endpoint: string, instance: string) => void
): (() => void) | null {
  const sub = UnifiedPush.addEndpointListener(({ endpoint, instance }) => {
    callback(endpoint, instance);
  });
  return sub ? () => sub.remove() : null;
}

/**
 * Listen for incoming UP messages and display as local notifications.
 */
export function onUPMessage(
  callback: (message: string, instance: string) => void
): (() => void) | null {
  const sub = UnifiedPush.addMessageListener(({ message, instance }) => {
    // Try to parse as JSON and show notification
    try {
      const parsed = JSON.parse(message);
      Notifications.scheduleNotificationAsync({
        content: {
          title: parsed.title || 'New notification',
          body: parsed.body || parsed.message || message,
          data: { type: 'unifiedpush', instance, raw: message },
        },
        trigger: null, // show immediately
      });
    } catch {
      // Plain text message
      Notifications.scheduleNotificationAsync({
        content: {
          title: 'Cross Dashboard',
          body: message,
          data: { type: 'unifiedpush', instance, raw: message },
        },
        trigger: null,
      });
    }
    callback(message, instance);
  });
  return sub ? () => sub.remove() : null;
}

export function onUPUnregistered(
  callback: (instance: string) => void
): (() => void) | null {
  const sub = UnifiedPush.addUnregisteredListener(({ instance }) => {
    callback(instance);
  });
  return sub ? () => sub.remove() : null;
}

export function onUPRegistrationFailed(
  callback: (instance: string) => void
): (() => void) | null {
  const sub = UnifiedPush.addRegistrationFailedListener(({ instance }) => {
    callback(instance);
  });
  return sub ? () => sub.remove() : null;
}

// --- Notification settings persistence ---

const NOTIF_ENABLED_KEY = 'notif_enabled' as CredentialKey;
const NOTIF_MINUTES_KEY = 'notif_minutes' as CredentialKey;
const UP_ENDPOINT_KEY = 'up_endpoint' as CredentialKey;

export async function getNotificationSettings(): Promise<{
  enabled: boolean;
  minutesBefore: number;
  upEndpoint: string | null;
}> {
  const [enabled, minutes, upEndpoint] = await Promise.all([
    getCredential(NOTIF_ENABLED_KEY),
    getCredential(NOTIF_MINUTES_KEY),
    getCredential(UP_ENDPOINT_KEY),
  ]);
  return {
    enabled: enabled === 'true',
    minutesBefore: minutes ? parseInt(minutes, 10) : 15,
    upEndpoint,
  };
}

export async function saveNotificationEnabled(enabled: boolean): Promise<void> {
  await setCredential(NOTIF_ENABLED_KEY, String(enabled));
}

export async function saveReminderMinutes(minutes: number): Promise<void> {
  await setCredential(NOTIF_MINUTES_KEY, String(minutes));
}

export async function saveUPEndpoint(endpoint: string): Promise<void> {
  await setCredential(UP_ENDPOINT_KEY, endpoint);
}
