import { useState, useCallback } from 'react';
import { Platform } from 'react-native';
import { useApp } from '../store/AppContext';
import { CalDavCalendar, CalendarEvent, GiteaIssue, CalDavTask } from '../types';
import * as caldav from '../services/caldav';
import * as gitea from '../services/gitea';
import * as cache from '../services/cache';
import * as notifications from '../services/notifications';
import * as DashboardWidget from '../../modules/dashboard-widget';

/**
 * Shared full-sync hook used by every screen's refresh button.
 * Fetches events, tasks, notes, and issues in parallel, respecting
 * selectedCalendars. Guards against clearing existing data when a
 * fetch returns empty (e.g. a transient network error).
 */
export function useSyncAll(): { syncAll: () => Promise<void>; syncing: boolean } {
  const { state, setEvents, setTasks, setNotes, setIssues, setLoading, setLastSync } = useApp();
  const [syncing, setSyncing] = useState(false);

  const syncAll = useCallback(async () => {
    if (syncing) return;
    setSyncing(true);
    setLoading(true);
    try {
      const fetches: Promise<void>[] = [];
      let freshEvents: CalendarEvent[] = state.events;
      let freshTasks: CalDavTask[] = state.tasks;
      let freshIssues: GiteaIssue[] = state.issues;

      if (state.caldavConfigured) {
        const hrefs = (type: string) => {
          const list = state.selectedCalendars
            .filter((c: CalDavCalendar) => c.components.includes(type))
            .map((c: CalDavCalendar) => c.href);
          return list.length > 0 ? list : undefined;
        };

        fetches.push(
          caldav.fetchEvents(hrefs('VEVENT')).then(async (events) => {
            if (events.length > 0 || state.events.length > 0) {
              freshEvents = events;
              setEvents(events);
              await cache.saveEvents(events);
            }
          })
        );
        fetches.push(
          caldav.fetchTasks(hrefs('VTODO')).then(async (tasks) => {
            if (tasks.length > 0 || state.tasks.length > 0) {
              freshTasks = tasks;
              setTasks(tasks);
              await cache.saveTasks(tasks);
            }
          })
        );
        fetches.push(
          caldav.fetchNotes(hrefs('VJOURNAL')).then(async (notes) => {
            if (notes.length > 0 || state.notes.length > 0) {
              setNotes(notes);
              await cache.saveNotes(notes);
            }
          })
        );
      }

      if (state.giteaConfigured && state.giteaRepositories.length > 0) {
        fetches.push(
          gitea.fetchAllIssues(state.giteaRepositories).then(async (issues) => {
            if (issues.length > 0 || state.issues.length > 0) {
              freshIssues = issues;
              setIssues(issues);
              await cache.saveIssues(issues);
            }
          })
        );
      }

      await Promise.all(fetches);
      const now = new Date();
      setLastSync(now);

      // Reschedule event and task reminders
      const notifSettings = await notifications.getNotificationSettings();
      if (notifSettings.enabled) {
        await notifications.scheduleEventReminders(freshEvents, notifSettings.minutesBefore);
        await notifications.scheduleTaskReminders(freshTasks, notifSettings.minutesBefore);
      }

      // Update home screen widget (Android only) — use fresh data from fetches
      if (Platform.OS === 'android' && DashboardWidget.isAvailable()) {
        const upcoming = freshEvents
          .filter((e) => e.start >= now)
          .sort((a, b) => a.start.getTime() - b.start.getTime())
          .slice(0, 3);

        const eventRows = upcoming.map((e) => {
          const time = e.start.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' });
          const date = e.start.toLocaleDateString([], { month: 'numeric', day: 'numeric' });
          return `${date} ${time} ${e.summary}`;
        });

        const pendingTasks = freshTasks
          .filter((t) => t.status !== 'COMPLETED' && t.status !== 'CANCELLED')
          .sort((a, b) => {
            const aDate = a.due?.getTime() ?? Infinity;
            const bDate = b.due?.getTime() ?? Infinity;
            return aDate - bDate;
          })
          .slice(0, 3);
        const taskRows = pendingTasks.map((t) => {
          const isOverdue = t.due && t.due < now;
          const prefix = isOverdue ? '⚠ ' : '• ';
          return `${prefix}${t.summary}`;
        });

        const openIssues = freshIssues.filter((i) => i.state === 'open');
        const syncLabel = `Synced ${now.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' })}`;

        DashboardWidget.updateWidgetData(
          eventRows.join('|'),
          taskRows.join('|'),
          openIssues.length,
          syncLabel
        );

        // Update 4x4 widget stats: overdue tasks, events remaining today, pomodoro sessions
        const endOfToday = new Date(now);
        endOfToday.setHours(23, 59, 59, 999);
        const eventsRemainingToday = freshEvents.filter(
          (e) => e.start >= now && e.start <= endOfToday
        ).length;

        const overdueTasks = freshTasks
          .filter((t) => t.status !== 'COMPLETED' && t.status !== 'CANCELLED' && t.due && t.due < now)
          .slice(0, 3);
        const overdueTaskRows = overdueTasks.map((t) => t.summary).join('|');

        const statsStore = await cache.loadStatsStore();
        const todayKey = `${now.getFullYear()}-${String(now.getMonth() + 1).padStart(2, '0')}-${String(now.getDate()).padStart(2, '0')}`;
        const todayStats = statsStore[todayKey] ?? { tasksCompleted: 0, pomodoroSessions: 0, issuesClosed: 0 };

        DashboardWidget.updateWidgetStats(
          eventsRemainingToday,
          todayStats.pomodoroSessions,
          overdueTaskRows
        );
      }
    } catch (error) {
      console.error('Error syncing data:', error);
    } finally {
      setSyncing(false);
      setLoading(false);
    }
  }, [state, setEvents, setTasks, setNotes, setIssues, setLoading, setLastSync, syncing]);

  return { syncAll, syncing };
}
