import React, { createContext, useContext, useReducer, useCallback, useEffect, ReactNode } from 'react';
import { CalendarEvent, Note, GiteaIssue, CalDavTask, CalDavCalendar } from '../types';
import { ThemePreference } from '../theme';
import * as cache from '../services/cache';
import { ScreenName, ALL_SCREENS } from '../services/cache';
import * as crypto from '../services/crypto';
import * as keyring from '../services/keyring';

interface AppState {
  events: CalendarEvent[];
  notes: Note[];
  issues: GiteaIssue[];
  tasks: CalDavTask[];
  isLoading: boolean;
  error: string | null;
  caldavConfigured: boolean;
  giteaConfigured: boolean;
  giteaRepositories: string[];
  selectedCalendars: CalDavCalendar[];
  themePreference: ThemePreference;
  visibleScreens: ScreenName[];
  lastSync: Date | null;
}

type AppAction =
  | { type: 'SET_EVENTS'; payload: CalendarEvent[] }
  | { type: 'SET_NOTES'; payload: Note[] }
  | { type: 'SET_ISSUES'; payload: GiteaIssue[] }
  | { type: 'SET_TASKS'; payload: CalDavTask[] }
  | { type: 'SET_LOADING'; payload: boolean }
  | { type: 'SET_ERROR'; payload: string | null }
  | { type: 'SET_CALDAV_CONFIGURED'; payload: boolean }
  | { type: 'SET_GITEA_CONFIGURED'; payload: boolean }
  | { type: 'SET_GITEA_REPOSITORIES'; payload: string[] }
  | { type: 'SET_SELECTED_CALENDARS'; payload: CalDavCalendar[] }
  | { type: 'SET_THEME'; payload: ThemePreference }
  | { type: 'SET_VISIBLE_SCREENS'; payload: ScreenName[] }
  | { type: 'SET_LAST_SYNC'; payload: Date | null };

const initialState: AppState = {
  events: [],
  notes: [],
  issues: [],
  tasks: [],
  isLoading: false,
  error: null,
  caldavConfigured: false,
  giteaConfigured: false,
  giteaRepositories: [],
  selectedCalendars: [],
  themePreference: 'system',
  visibleScreens: ALL_SCREENS,
  lastSync: null,
};

function appReducer(state: AppState, action: AppAction): AppState {
  switch (action.type) {
    case 'SET_EVENTS':
      return { ...state, events: action.payload };
    case 'SET_NOTES':
      return { ...state, notes: action.payload };
    case 'SET_ISSUES':
      return { ...state, issues: action.payload };
    case 'SET_TASKS':
      return { ...state, tasks: action.payload };
    case 'SET_LOADING':
      return { ...state, isLoading: action.payload };
    case 'SET_ERROR':
      return { ...state, error: action.payload };
    case 'SET_CALDAV_CONFIGURED':
      return { ...state, caldavConfigured: action.payload };
    case 'SET_GITEA_CONFIGURED':
      return { ...state, giteaConfigured: action.payload };
    case 'SET_GITEA_REPOSITORIES':
      return { ...state, giteaRepositories: action.payload };
    case 'SET_SELECTED_CALENDARS':
      return { ...state, selectedCalendars: action.payload };
    case 'SET_THEME':
      return { ...state, themePreference: action.payload };
    case 'SET_VISIBLE_SCREENS':
      return { ...state, visibleScreens: action.payload };
    case 'SET_LAST_SYNC':
      return { ...state, lastSync: action.payload };
    default:
      return state;
  }
}

interface AppContextType {
  state: AppState;
  setEvents: (events: CalendarEvent[]) => void;
  setNotes: (notes: Note[]) => void;
  setIssues: (issues: GiteaIssue[]) => void;
  setTasks: (tasks: CalDavTask[]) => void;
  setLoading: (loading: boolean) => void;
  setError: (error: string | null) => void;
  setCaldavConfigured: (configured: boolean) => void;
  setGiteaConfigured: (configured: boolean) => void;
  setGiteaRepositories: (repos: string[]) => void;
  setSelectedCalendars: (calendars: CalDavCalendar[]) => void;
  setThemePreference: (theme: ThemePreference) => void;
  setVisibleScreens: (screens: ScreenName[]) => void;
  setLastSync: (date: Date | null) => void;
}

const AppContext = createContext<AppContextType | undefined>(undefined);

export function AppProvider({ children }: { children: ReactNode }) {
  const [state, dispatch] = useReducer(appReducer, initialState);

  useEffect(() => {
    async function init() {
      // Initialize encryption key — may fail on Android if Keystore is unavailable
      try {
        await crypto.initEncryptionKey();
      } catch (error) {
        console.warn('Failed to initialize encryption key:', error);
      }

      // Phase 1: Load from AsyncStorage (always reliable, no Keystore dependency).
      // This must be separate from the SecureStore loads so that visible screens
      // and theme are always restored even when the Android Keystore is unavailable.
      const [themeRaw, savedScreens] = await Promise.all([
        cache.loadThemePreference(),
        cache.loadVisibleScreens(),
      ]);
      if (themeRaw === 'light' || themeRaw === 'dark' || themeRaw === 'system') {
        dispatch({ type: 'SET_THEME', payload: themeRaw });
      }
      if (savedScreens) {
        dispatch({ type: 'SET_VISIBLE_SCREENS', payload: savedScreens });
      }

      // Phase 2: Load from SecureStore/Keychain — may fail on Android when the
      // Keystore becomes temporarily unavailable (e.g. after removing device lock).
      try {
        const [caldavServer, caldavUsername, caldavPassword,
          giteaInstance, giteaToken, savedCalendars, savedGiteaRepos] = await Promise.all([
          keyring.getCredential('caldav_server'),
          keyring.getCredential('caldav_username'),
          keyring.getCredential('caldav_password'),
          keyring.getCredential('gitea_instance'),
          keyring.getCredential('gitea_token'),
          keyring.getCredential('caldav_selected_calendars'),
          keyring.getCredential('gitea_repositories'),
        ]);

        // Restore CalDAV configured state
        if (caldavServer && caldavUsername && caldavPassword) {
          dispatch({ type: 'SET_CALDAV_CONFIGURED', payload: true });
        }

        // Restore selected calendars
        if (savedCalendars) {
          try {
            const cals: CalDavCalendar[] = JSON.parse(savedCalendars);
            dispatch({ type: 'SET_SELECTED_CALENDARS', payload: cals });
          } catch { /* ignore parse errors */ }
        }

        // Restore Gitea configured state and repositories
        if (giteaInstance && giteaToken) {
          dispatch({ type: 'SET_GITEA_CONFIGURED', payload: true });
        }
        if (savedGiteaRepos) {
          try {
            const repos: string[] = JSON.parse(savedGiteaRepos);
            dispatch({ type: 'SET_GITEA_REPOSITORIES', payload: repos });
          } catch { /* ignore parse errors */ }
        }
      } catch (error) {
        console.warn('Failed to load credentials from secure store:', error);
      }

      // Phase 3: Load encrypted cache data
      try {
        const [events, notes, issues, tasks, lastSync] = await Promise.all([
          cache.loadEvents(),
          cache.loadNotes(),
          cache.loadIssues(),
          cache.loadTasks(),
          cache.getLastSync(),
        ]);

        if (events) dispatch({ type: 'SET_EVENTS', payload: events });
        if (notes) dispatch({ type: 'SET_NOTES', payload: notes });
        if (issues) dispatch({ type: 'SET_ISSUES', payload: issues });
        if (tasks) dispatch({ type: 'SET_TASKS', payload: tasks });
        if (lastSync) dispatch({ type: 'SET_LAST_SYNC', payload: lastSync });
      } catch {
        console.warn('Failed to decrypt cached data, clearing cache');
        await cache.clearCache();
      }
    }
    init();
  }, []);

  const setEvents = useCallback((events: CalendarEvent[]) => {
    dispatch({ type: 'SET_EVENTS', payload: events });
  }, []);

  const setNotes = useCallback((notes: Note[]) => {
    dispatch({ type: 'SET_NOTES', payload: notes });
  }, []);

  const setIssues = useCallback((issues: GiteaIssue[]) => {
    dispatch({ type: 'SET_ISSUES', payload: issues });
  }, []);

  const setTasks = useCallback((tasks: CalDavTask[]) => {
    dispatch({ type: 'SET_TASKS', payload: tasks });
  }, []);

  const setLoading = useCallback((loading: boolean) => {
    dispatch({ type: 'SET_LOADING', payload: loading });
  }, []);

  const setError = useCallback((error: string | null) => {
    dispatch({ type: 'SET_ERROR', payload: error });
  }, []);

  const setCaldavConfigured = useCallback((configured: boolean) => {
    dispatch({ type: 'SET_CALDAV_CONFIGURED', payload: configured });
  }, []);

  const setGiteaConfigured = useCallback((configured: boolean) => {
    dispatch({ type: 'SET_GITEA_CONFIGURED', payload: configured });
  }, []);

  const setGiteaRepositories = useCallback((repos: string[]) => {
    dispatch({ type: 'SET_GITEA_REPOSITORIES', payload: repos });
  }, []);

  const setSelectedCalendars = useCallback((calendars: CalDavCalendar[]) => {
    dispatch({ type: 'SET_SELECTED_CALENDARS', payload: calendars });
  }, []);

  const setThemePreference = useCallback((theme: ThemePreference) => {
    dispatch({ type: 'SET_THEME', payload: theme });
    cache.saveThemePreference(theme);
  }, []);

  const setVisibleScreens = useCallback((screens: ScreenName[]) => {
    dispatch({ type: 'SET_VISIBLE_SCREENS', payload: screens });
    cache.saveVisibleScreens(screens);
  }, []);

  const setLastSync = useCallback((date: Date | null) => {
    dispatch({ type: 'SET_LAST_SYNC', payload: date });
  }, []);

  const value: AppContextType = {
    state,
    setEvents,
    setNotes,
    setIssues,
    setTasks,
    setLoading,
    setError,
    setCaldavConfigured,
    setGiteaConfigured,
    setGiteaRepositories,
    setSelectedCalendars,
    setThemePreference,
    setVisibleScreens,
    setLastSync,
  };

  return <AppContext.Provider value={value}>{children}</AppContext.Provider>;
}

export function useApp(): AppContextType {
  const context = useContext(AppContext);
  if (context === undefined) {
    throw new Error('useApp must be used within an AppProvider');
  }
  return context;
}
