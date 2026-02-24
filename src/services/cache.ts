import AsyncStorage from '@react-native-async-storage/async-storage';
import { CalendarEvent, Note, GiteaIssue, CalDavTask } from '../types';
import { encrypt, decrypt } from './crypto';

const CACHE_EVENTS = '@cache/events';
const CACHE_NOTES = '@cache/notes';
const CACHE_ISSUES = '@cache/issues';
const CACHE_TASKS = '@cache/tasks';
const CACHE_LAST_SYNC = '@cache/last_sync';
const CACHE_THEME = '@cache/theme';

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

export async function clearCache(): Promise<void> {
  await AsyncStorage.multiRemove([CACHE_EVENTS, CACHE_NOTES, CACHE_ISSUES, CACHE_TASKS, CACHE_LAST_SYNC]);
}
