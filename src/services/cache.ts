import AsyncStorage from '@react-native-async-storage/async-storage';
import { CalendarEvent, Note, GiteaIssue } from '../types';

const CACHE_EVENTS = '@cache/events';
const CACHE_NOTES = '@cache/notes';
const CACHE_ISSUES = '@cache/issues';
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

function reviveIssueDates(raw: unknown): GiteaIssue {
  const i = raw as Record<string, unknown>;
  return {
    ...i,
    createdAt: new Date(i.createdAt as string),
    updatedAt: new Date(i.updatedAt as string),
  } as GiteaIssue;
}

export async function saveEvents(events: CalendarEvent[]): Promise<void> {
  await Promise.all([
    AsyncStorage.setItem(CACHE_EVENTS, JSON.stringify(events)),
    AsyncStorage.setItem(CACHE_LAST_SYNC, new Date().toISOString()),
  ]);
}

export async function loadEvents(): Promise<CalendarEvent[] | null> {
  const data = await AsyncStorage.getItem(CACHE_EVENTS);
  if (!data) return null;
  return (JSON.parse(data) as unknown[]).map(reviveEventDates);
}

export async function saveNotes(notes: Note[]): Promise<void> {
  await AsyncStorage.setItem(CACHE_NOTES, JSON.stringify(notes));
}

export async function loadNotes(): Promise<Note[] | null> {
  const data = await AsyncStorage.getItem(CACHE_NOTES);
  if (!data) return null;
  return (JSON.parse(data) as unknown[]).map(reviveNoteDates);
}

export async function saveIssues(issues: GiteaIssue[]): Promise<void> {
  await AsyncStorage.setItem(CACHE_ISSUES, JSON.stringify(issues));
}

export async function loadIssues(): Promise<GiteaIssue[] | null> {
  const data = await AsyncStorage.getItem(CACHE_ISSUES);
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
  await AsyncStorage.multiRemove([CACHE_EVENTS, CACHE_NOTES, CACHE_ISSUES, CACHE_LAST_SYNC]);
}
