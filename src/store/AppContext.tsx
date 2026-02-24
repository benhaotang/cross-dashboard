import React, { createContext, useContext, useReducer, useCallback, ReactNode } from 'react';
import { CalendarEvent, Note, GiteaIssue } from '../types';

interface AppState {
  events: CalendarEvent[];
  notes: Note[];
  issues: GiteaIssue[];
  isLoading: boolean;
  error: string | null;
  caldavConfigured: boolean;
  giteaConfigured: boolean;
  giteaRepositories: string[];
}

type AppAction =
  | { type: 'SET_EVENTS'; payload: CalendarEvent[] }
  | { type: 'SET_NOTES'; payload: Note[] }
  | { type: 'SET_ISSUES'; payload: GiteaIssue[] }
  | { type: 'SET_LOADING'; payload: boolean }
  | { type: 'SET_ERROR'; payload: string | null }
  | { type: 'SET_CALDAV_CONFIGURED'; payload: boolean }
  | { type: 'SET_GITEA_CONFIGURED'; payload: boolean }
  | { type: 'SET_GITEA_REPOSITORIES'; payload: string[] };

const initialState: AppState = {
  events: [],
  notes: [],
  issues: [],
  isLoading: false,
  error: null,
  caldavConfigured: false,
  giteaConfigured: false,
  giteaRepositories: [],
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
}

const AppContext = createContext<AppContextType | undefined>(undefined);

export function AppProvider({ children }: { children: ReactNode }) {
  const [state, dispatch] = useReducer(appReducer, initialState);

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
