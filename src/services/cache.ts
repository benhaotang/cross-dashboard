import AsyncStorage from '@react-native-async-storage/async-storage';
import { CalendarEvent, Note, GiteaIssue, CalDavTask } from '../types';
import { TaskDefaults } from './taskParser';
import { encrypt, decrypt } from './crypto';

const CACHE_EVENTS = '@cache/events';
const CACHE_NOTES = '@cache/notes';
const CACHE_ISSUES = '@cache/issues';
const CACHE_TASKS = '@cache/tasks';
const CACHE_LAST_SYNC = '@cache/last_sync';
const CACHE_THEME = '@cache/theme';
const CACHE_TASK_DEFAULTS = '@cache/task_defaults';
const CACHE_POMODORO = '@cache/pomodoro_settings';
const CACHE_VISIBLE_SCREENS = '@cache/visible_screens';
const CACHE_KANBAN_COLUMNS = '@cache/kanban_columns';
const CACHE_DAILY_STATS = '@cache/daily_stats';

export type ScreenName = 'Dashboard' | 'Inbox' | 'Events' | 'Notes' | 'Tasks' | 'Issues' | 'Views';

export const ALL_SCREENS: ScreenName[] = ['Dashboard', 'Inbox', 'Events', 'Notes', 'Tasks', 'Issues', 'Views'];

export interface PomodoroSettings {
  workMinutes: number;
  shortBreakMinutes: number;
  longBreakMinutes: number;
  sessionsUntilLongBreak: number;
}

export const DEFAULT_POMODORO: PomodoroSettings = {
  workMinutes: 25,
  shortBreakMinutes: 5,
  longBreakMinutes: 15,
  sessionsUntilLongBreak: 4,
};

function reviveEventDates(raw: unknown): CalendarEvent {
  const e = raw as Record<string, unknown>;
  return { ...e, start: new Date(e.start as string), end: new Date(e.end as string) } as CalendarEvent;
}

function reviveNoteDates(raw: unknown): Note {
  const n = raw as Record<string, unknown>;
  return {
    ...n,
    createdAt: new Date(n.createdAt as string),
    updatedAt: new Date(n.updatedAt as string),
  } as Note;
}

function reviveTaskDates(raw: unknown): CalDavTask {
  const t = raw as Record<string, unknown>;
  return {
    ...t,
    due: t.due ? new Date(t.due as string) : undefined,
    dtstart: t.dtstart ? new Date(t.dtstart as string) : undefined,
    completed: t.completed ? new Date(t.completed as string) : undefined,
    created: new Date(t.created as string),
    lastModified: new Date(t.lastModified as string),
  } as CalDavTask;
}

function reviveIssueDates(raw: unknown): GiteaIssue {
  const i = raw as Record<string, unknown>;
  return {
    ...i,
    createdAt: new Date(i.createdAt as string),
    updatedAt: new Date(i.updatedAt as string),
  } as GiteaIssue;
}

async function encryptedSetItem(key: string, json: string): Promise<void> {
  const encrypted = encrypt(json);
  await AsyncStorage.setItem(key, encrypted);
}

async function decryptedGetItem(key: string): Promise<string | null> {
  const stored = await AsyncStorage.getItem(key);
  if (!stored) return null;
  return decrypt(stored);
}

export async function saveEvents(events: CalendarEvent[]): Promise<void> {
  await Promise.all([
    encryptedSetItem(CACHE_EVENTS, JSON.stringify(events)),
    AsyncStorage.setItem(CACHE_LAST_SYNC, new Date().toISOString()),
  ]);
}

export async function loadEvents(): Promise<CalendarEvent[] | null> {
  const data = await decryptedGetItem(CACHE_EVENTS);
  if (!data) return null;
  return (JSON.parse(data) as unknown[]).map(reviveEventDates);
}

export async function saveNotes(notes: Note[]): Promise<void> {
  await encryptedSetItem(CACHE_NOTES, JSON.stringify(notes));
}

export async function loadNotes(): Promise<Note[] | null> {
  const data = await decryptedGetItem(CACHE_NOTES);
  if (!data) return null;
  return (JSON.parse(data) as unknown[]).map(reviveNoteDates);
}

export async function saveTasks(tasks: CalDavTask[]): Promise<void> {
  await encryptedSetItem(CACHE_TASKS, JSON.stringify(tasks));
}

export async function loadTasks(): Promise<CalDavTask[] | null> {
  const data = await decryptedGetItem(CACHE_TASKS);
  if (!data) return null;
  return (JSON.parse(data) as unknown[]).map(reviveTaskDates);
}

export async function saveIssues(issues: GiteaIssue[]): Promise<void> {
  await encryptedSetItem(CACHE_ISSUES, JSON.stringify(issues));
}

export async function loadIssues(): Promise<GiteaIssue[] | null> {
  const data = await decryptedGetItem(CACHE_ISSUES);
  if (!data) return null;
  return (JSON.parse(data) as unknown[]).map(reviveIssueDates);
}

export async function getLastSync(): Promise<Date | null> {
  const data = await AsyncStorage.getItem(CACHE_LAST_SYNC);
  return data ? new Date(data) : null;
}

export async function saveThemePreference(theme: string): Promise<void> {
  await AsyncStorage.setItem(CACHE_THEME, theme);
}

export async function loadThemePreference(): Promise<string | null> {
  return AsyncStorage.getItem(CACHE_THEME);
}

export async function saveTaskDefaults(defaults: TaskDefaults): Promise<void> {
  await AsyncStorage.setItem(CACHE_TASK_DEFAULTS, JSON.stringify(defaults));
}

export async function loadTaskDefaults(): Promise<TaskDefaults | null> {
  const data = await AsyncStorage.getItem(CACHE_TASK_DEFAULTS);
  if (!data) return null;
  return JSON.parse(data) as TaskDefaults;
}

export async function savePomodoroSettings(settings: PomodoroSettings): Promise<void> {
  await AsyncStorage.setItem(CACHE_POMODORO, JSON.stringify(settings));
}

export async function loadPomodoroSettings(): Promise<PomodoroSettings | null> {
  const data = await AsyncStorage.getItem(CACHE_POMODORO);
  if (!data) return null;
  return JSON.parse(data) as PomodoroSettings;
}

export async function saveVisibleScreens(screens: ScreenName[]): Promise<void> {
  await AsyncStorage.setItem(CACHE_VISIBLE_SCREENS, JSON.stringify(screens));
}

export async function loadVisibleScreens(): Promise<ScreenName[] | null> {
  const data = await AsyncStorage.getItem(CACHE_VISIBLE_SCREENS);
  if (!data) return null;
  return JSON.parse(data) as ScreenName[];
}

// ─── Daily Activity Stats ────────────────────────────────────────────────────

export interface DailyStats {
  tasksCompleted: number;
  pomodoroSessions: number;
  issuesClosed: number;
}

/** All days we have records for, keyed by 'YYYY-MM-DD'. */
export type StatsStore = Record<string, DailyStats>;

function dateKey(d: Date = new Date()): string {
  return `${d.getFullYear()}-${String(d.getMonth() + 1).padStart(2, '0')}-${String(d.getDate()).padStart(2, '0')}`;
}

export async function loadStatsStore(): Promise<StatsStore> {
  const data = await AsyncStorage.getItem(CACHE_DAILY_STATS);
  if (!data) return {};
  return JSON.parse(data) as StatsStore;
}

async function saveStatsStore(store: StatsStore): Promise<void> {
  await AsyncStorage.setItem(CACHE_DAILY_STATS, JSON.stringify(store));
}

/** Atomically increment one counter for today. Fire-and-forget safe. */
export async function incrementStat(stat: keyof DailyStats): Promise<void> {
  const store = await loadStatsStore();
  const key = dateKey();
  const today = store[key] ?? { tasksCompleted: 0, pomodoroSessions: 0, issuesClosed: 0 };
  store[key] = { ...today, [stat]: today[stat] + 1 };
  await saveStatsStore(store);
}

/** Sum one stat across a range of days. daysAgo=0 means today. */
export function sumStatRange(store: StatsStore, startDaysAgo: number, count: number): DailyStats {
  let tasksCompleted = 0, pomodoroSessions = 0, issuesClosed = 0;
  const now = new Date();
  for (let i = startDaysAgo; i < startDaysAgo + count; i++) {
    const d = new Date(now);
    d.setDate(d.getDate() - i);
    const day = store[dateKey(d)];
    if (day) {
      tasksCompleted += day.tasksCompleted;
      pomodoroSessions += day.pomodoroSessions;
      issuesClosed += day.issuesClosed;
    }
  }
  return { tasksCompleted, pomodoroSessions, issuesClosed };
}

// ─────────────────────────────────────────────────────────────────────────────

export async function saveKanbanColumns(columns: string[]): Promise<void> {
  await AsyncStorage.setItem(CACHE_KANBAN_COLUMNS, JSON.stringify(columns));
}

export async function loadKanbanColumns(): Promise<string[] | null> {
  const data = await AsyncStorage.getItem(CACHE_KANBAN_COLUMNS);
  if (!data) return null;
  return JSON.parse(data) as string[];
}

export async function clearCache(): Promise<void> {
  await AsyncStorage.multiRemove([CACHE_EVENTS, CACHE_NOTES, CACHE_ISSUES, CACHE_TASKS, CACHE_TASK_DEFAULTS, CACHE_LAST_SYNC]);
}
