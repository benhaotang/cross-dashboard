import { useState, useCallback } from 'react';
import { useApp } from '../store/AppContext';
import { CalDavCalendar } from '../types';
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
              setEvents(events);
              await cache.saveEvents(events);
            }
          })
        );
        fetches.push(
          caldav.fetchTasks(hrefs('VTODO')).then(async (tasks) => {
            if (tasks.length > 0 || state.tasks.length > 0) {
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
              setIssues(issues);
              await cache.saveIssues(issues);
            }
          })
        );
      }

      await Promise.all(fetches);
      const now = new Date();
      setLastSync(now);

      // Reschedule event reminders
      const notifSettings = await notifications.getNotificationSettings();
      if (notifSettings.enabled && state.events.length > 0) {
        await notifications.scheduleEventReminders(state.events, notifSettings.minutesBefore);
      }

      // Update home screen widget
      if (DashboardWidget.isAvailable()) {
        const upcomingCount = state.events.filter((e) => e.start >= now).length;
        const openCount = state.issues.filter((i) => i.state === 'open').length;
        const nextEvt = state.events
          .filter((e) => e.start >= now)
          .sort((a, b) => a.start.getTime() - b.start.getTime())[0];
        const nextEventText = nextEvt
          ? `${nextEvt.summary} - ${nextEvt.start.toLocaleDateString()} ${nextEvt.start.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' })}`
          : 'No upcoming events';
        DashboardWidget.updateWidgetData(
          upcomingCount,
          openCount,
          nextEventText,
          `Synced ${now.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' })}`
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
