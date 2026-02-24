import { Platform } from 'react-native';
import * as Notifications from 'expo-notifications';
import { CalendarEvent, CalDavTask } from '../types';
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
  if (Platform.OS === 'web') return false;
  const { status } = await Notifications.requestPermissionsAsync();
  return status === 'granted';
}

export async function scheduleEventReminders(
  events: CalendarEvent[],
  minutesBefore: number = 15
): Promise<void> {
  // Cancel all existing event reminders first
  await cancelAllEventReminders();

  const now = new Date();
  const triggerOffset = minutesBefore * 60;

  // remind-before notifications
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

  // at-time notifications
  for (const event of events) {
    if (event.start <= now) continue;

    await Notifications.scheduleNotificationAsync({
      content: {
        title: event.summary,
        body: 'Starting now' + (event.location ? ` at ${event.location}` : ''),
        data: { type: 'event_at_time', uid: event.uid },
      },
      trigger: {
        type: Notifications.SchedulableTriggerInputTypes.DATE,
        date: event.start,
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
  await Notifications.cancelAllScheduledNotificationsAsync();
}

// --- Task due-date notifications ---
// Must be called AFTER scheduleEventReminders (which cancels all first).

export async function scheduleTaskReminders(
  tasks: CalDavTask[],
  minutesBefore: number = 15
): Promise<void> {
  const now = new Date();
  const triggerOffsetMs = minutesBefore * 60 * 1000;

  for (const task of tasks) {
    if (!task.due) continue;
    if (task.status === 'COMPLETED' || task.status === 'CANCELLED') continue;
    if (task.due <= now) continue;

    // remind-before
    if (minutesBefore > 0) {
      const remindTime = new Date(task.due.getTime() - triggerOffsetMs);
      if (remindTime > now) {
        await Notifications.scheduleNotificationAsync({
          content: {
            title: task.summary,
            body: `Due in ${minutesBefore} minutes`,
            data: { type: 'task_reminder', uid: task.uid },
          },
          trigger: {
            type: Notifications.SchedulableTriggerInputTypes.DATE,
            date: remindTime,
          },
        });
      }
    }

    // at-due
    await Notifications.scheduleNotificationAsync({
      content: {
        title: task.summary,
        body: 'Due now',
        data: { type: 'task_due', uid: task.uid },
      },
      trigger: {
        type: Notifications.SchedulableTriggerInputTypes.DATE,
        date: task.due,
      },
    });
  }
}

// --- Notification settings persistence ---

const NOTIF_ENABLED_KEY = 'notif_enabled' as CredentialKey;
const NOTIF_MINUTES_KEY = 'notif_minutes' as CredentialKey;

export async function getNotificationSettings(): Promise<{
  enabled: boolean;
  minutesBefore: number;
}> {
  const [enabled, minutes] = await Promise.all([
    getCredential(NOTIF_ENABLED_KEY),
    getCredential(NOTIF_MINUTES_KEY),
  ]);
  return {
    enabled: enabled === 'true',
    minutesBefore: minutes ? parseInt(minutes, 10) : 15,
  };
}

export async function saveNotificationEnabled(enabled: boolean): Promise<void> {
  await setCredential(NOTIF_ENABLED_KEY, String(enabled));
}

export async function saveReminderMinutes(minutes: number): Promise<void> {
  await setCredential(NOTIF_MINUTES_KEY, String(minutes));
}
