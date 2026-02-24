import React, { createContext, useContext, useReducer, useCallback, useEffect, ReactNode } from 'react';
import { CalendarEvent, Note, GiteaIssue } from '../types';
import { ThemePreference } from '../theme';
import * as cache from '../services/cache';

interface AppState {
  events: CalendarEvent[];
  notes: Note[];
  issues: GiteaIssue[];
  isLoading: boolean;
  error: string | null;
  caldavConfigured: boolean;
  giteaConfigured: boolean;
  giteaRepositories: string[];
  themePreference: ThemePreference;
  lastSync: Date | null;
}

type AppAction =
  | { type: 'SET_EVENTS'; payload: CalendarEvent[] }
  | { type: 'SET_NOTES'; payload: Note[] }
  | { type: 'SET_ISSUES'; payload: GiteaIssue[] }
  | { type: 'SET_LOADING'; payload: boolean }
  | { type: 'SET_ERROR'; payload: string | null }
  | { type: 'SET_CALDAV_CONFIGURED'; payload: boolean }
  | { type: 'SET_GITEA_CONFIGURED'; payload: boolean }
  | { type: 'SET_GITEA_REPOSITORIES'; payload: string[] }
  | { type: 'SET_THEME'; payload: ThemePreference }
  | { type: 'SET_LAST_SYNC'; payload: Date | null };

const initialState: AppState = {
  events: [],
  notes: [],
  issues: [],
  isLoading: false,
  error: null,
  caldavConfigured: false,
  giteaConfigured: false,
  giteaRepositories: [],
  themePreference: 'system',
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
    case 'SET_THEME':
      return { ...state, themePreference: action.payload };
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
  setLoading: (loading: boolean) => void;
  setError: (error: string | null) => void;
  setCaldavConfigured: (configured: boolean) => void;
  setGiteaConfigured: (configured: boolean) => void;
  setGiteaRepositories: (repos: string[]) => void;
  setThemePreference: (theme: ThemePreference) => void;
  setLastSync: (date: Date | null) => void;
}

const AppContext = createContext<AppContextType | undefined>(undefined);

export function AppProvider({ children }: { children: ReactNode }) {
  const [state, dispatch] = useReducer(appReducer, initialState);

  useEffect(() => {
    async function init() {
      const [themeRaw, events, notes, issues, lastSync] = await Promise.all([
        cache.loadThemePreference(),
        cache.loadEvents(),
        cache.loadNotes(),
        cache.loadIssues(),
        cache.getLastSync(),
      ]);

      if (themeRaw === 'light' || themeRaw === 'dark' || themeRaw === 'system') {
        dispatch({ type: 'SET_THEME', payload: themeRaw });
      }
      if (events) dispatch({ type: 'SET_EVENTS', payload: events });
      if (notes) dispatch({ type: 'SET_NOTES', payload: notes });
      if (issues) dispatch({ type: 'SET_ISSUES', payload: issues });
      if (lastSync) dispatch({ type: 'SET_LAST_SYNC', payload: lastSync });
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

  const setThemePreference = useCallback((theme: ThemePreference) => {
    dispatch({ type: 'SET_THEME', payload: theme });
    cache.saveThemePreference(theme);
  }, []);

  const setLastSync = useCallback((date: Date | null) => {
    dispatch({ type: 'SET_LAST_SYNC', payload: date });
  }, []);

  const value: AppContextType = {
    state,
    setEvents,
    setNotes,
    setIssues,
    setLoading,
    setError,
    setCaldavConfigured,
    setGiteaConfigured,
    setGiteaRepositories,
    setThemePreference,
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
