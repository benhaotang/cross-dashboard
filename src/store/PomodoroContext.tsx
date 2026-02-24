import React, { createContext, useContext, useState, useEffect, useRef, useCallback, ReactNode } from 'react';
import { Alert, Platform } from 'react-native';
import { loadPomodoroSettings, DEFAULT_POMODORO, PomodoroSettings } from '../services/cache';

export type TimerPhase = 'work' | 'shortBreak' | 'longBreak';

interface PomodoroState {
  phase: TimerPhase;
  secondsLeft: number;
  running: boolean;
  currentSession: number;
  completedSessions: number;
  settings: PomodoroSettings;
  itemTitle: string;
  active: boolean;
  modalVisible: boolean;
}

interface PomodoroContextType extends PomodoroState {
  start: (itemTitle: string, onSessionComplete?: (session: number, totalMinutes: number) => void) => void;
  pause: () => void;
  resume: () => void;
  stop: () => void;
  openModal: () => void;
  closeModal: () => void;
}

const PomodoroContext = createContext<PomodoroContextType | undefined>(undefined);

export function PomodoroProvider({ children }: { children: ReactNode }) {
  const [settings, setSettings] = useState<PomodoroSettings>(DEFAULT_POMODORO);
  const [phase, setPhase] = useState<TimerPhase>('work');
  const [secondsLeft, setSecondsLeft] = useState(DEFAULT_POMODORO.workMinutes * 60);
  const [running, setRunning] = useState(false);
  const [currentSession, setCurrentSession] = useState(1);
  const [completedSessions, setCompletedSessions] = useState(0);
  const [itemTitle, setItemTitle] = useState('');
  const [active, setActive] = useState(false);
  const [modalVisible, setModalVisible] = useState(false);

  const intervalRef = useRef<ReturnType<typeof setInterval> | null>(null);
  const settingsRef = useRef(settings);
  const phaseRef = useRef(phase);
  const currentSessionRef = useRef(currentSession);
  const completedSessionsRef = useRef(completedSessions);
  const onSessionCompleteRef = useRef<((session: number, totalMinutes: number) => void) | undefined>(undefined);
  const nativeListenersRef = useRef<Array<{ remove(): void }>>([]);

  settingsRef.current = settings;
  phaseRef.current = phase;
  currentSessionRef.current = currentSession;
  completedSessionsRef.current = completedSessions;

  const handleTimerEnd = useCallback(() => {
    const s = settingsRef.current;
    if (phaseRef.current === 'work') {
      const newCompleted = completedSessionsRef.current + 1;
      setCompletedSessions(newCompleted);
      onSessionCompleteRef.current?.(newCompleted, s.workMinutes);

      const isLongBreak = newCompleted % s.sessionsUntilLongBreak === 0;
      if (isLongBreak) {
        setPhase('longBreak');
        const secs = s.longBreakMinutes * 60;
        setSecondsLeft(secs);
        syncNativePhase('longBreak', secs);
      } else {
        setPhase('shortBreak');
        const secs = s.shortBreakMinutes * 60;
        setSecondsLeft(secs);
        syncNativePhase('shortBreak', secs);
      }
    } else {
      setCurrentSession(currentSessionRef.current + 1);
      setPhase('work');
      const secs = s.workMinutes * 60;
      setSecondsLeft(secs);
      syncNativePhase('work', secs);
    }
  }, []);

  // JS interval tick
  useEffect(() => {
    if (running) {
      intervalRef.current = setInterval(() => {
        setSecondsLeft((prev) => {
          if (prev <= 1) {
            handleTimerEnd();
            return 0;
          }
          return prev - 1;
        });
      }, 1000);
    } else if (intervalRef.current) {
      clearInterval(intervalRef.current);
      intervalRef.current = null;
    }
    return () => {
      if (intervalRef.current) clearInterval(intervalRef.current);
    };
  }, [running, handleTimerEnd]);

  // Android: check SCHEDULE_EXACT_ALARM permission on mount and prompt if missing.
  // This permission is required on Android 12+ for the backup AlarmManager alarm
  // to fire while the app is in the background (shown as "Wecker und Erinnerungen"
  // in system settings on German devices).
  useEffect(() => {
    if (Platform.OS !== 'android') return;
    try {
      const PomodoroService = require('../../modules/pomodoro-service');
      if (PomodoroService?.isAvailable() && !PomodoroService.canScheduleExactAlarms()) {
        Alert.alert(
          'Alarm Permission Needed',
          'To keep the Pomodoro timer running while the app is in the background, please allow "Alarms & Reminders" (Wecker und Erinnerungen) for this app.',
          [
            { text: 'Later', style: 'cancel' },
            {
              text: 'Open Settings',
              onPress: () => PomodoroService.requestExactAlarmPermission(),
            },
          ],
        );
      }
    } catch { /* module not available */ }
  }, []);

  // Android native module integration
  useEffect(() => {
    if (Platform.OS !== 'android') return;

    let PomodoroService: typeof import('../../modules/pomodoro-service') | null = null;
    try {
      PomodoroService = require('../../modules/pomodoro-service');
    } catch {
      return;
    }
    if (!PomodoroService || !PomodoroService.isAvailable()) return;

    const subs: Array<{ remove(): void }> = [];

    const tickSub = PomodoroService.addTickListener((e) => {
      setSecondsLeft(e.secondsLeft);
    });
    if (tickSub) subs.push(tickSub);

    const phaseEndSub = PomodoroService.addPhaseEndListener(() => {
      handleTimerEnd();
    });
    if (phaseEndSub) subs.push(phaseEndSub);

    const actionSub = PomodoroService.addActionListener((e) => {
      if (e.action === 'pause') {
        setRunning(false);
      } else if (e.action === 'resume') {
        setRunning(true);
      } else if (e.action === 'stop') {
        setRunning(false);
        setActive(false);
        setModalVisible(false);
        setPhase('work');
        setSecondsLeft(settingsRef.current.workMinutes * 60);
        setCurrentSession(1);
        setCompletedSessions(0);
      }
    });
    if (actionSub) subs.push(actionSub);

    nativeListenersRef.current = subs;

    return () => {
      subs.forEach((s) => s.remove());
      nativeListenersRef.current = [];
    };
  }, [handleTimerEnd]);

  function syncNativeStart(seconds: number, taskName: string, timerPhase: TimerPhase) {
    if (Platform.OS !== 'android') return;
    try {
      const PomodoroService = require('../../modules/pomodoro-service');
      if (PomodoroService?.isAvailable()) {
        PomodoroService.startTimer(seconds, taskName, timerPhase);
      }
    } catch {}
  }

  function syncNativePause() {
    if (Platform.OS !== 'android') return;
    try {
      const PomodoroService = require('../../modules/pomodoro-service');
      if (PomodoroService?.isAvailable()) {
        PomodoroService.pauseTimer();
      }
    } catch {}
  }

  function syncNativeResume() {
    if (Platform.OS !== 'android') return;
    try {
      const PomodoroService = require('../../modules/pomodoro-service');
      if (PomodoroService?.isAvailable()) {
        PomodoroService.resumeTimer();
      }
    } catch {}
  }

  function syncNativeStop() {
    if (Platform.OS !== 'android') return;
    try {
      const PomodoroService = require('../../modules/pomodoro-service');
      if (PomodoroService?.isAvailable()) {
        PomodoroService.stopTimer();
      }
    } catch {}
  }

  function syncNativePhase(timerPhase: TimerPhase, seconds: number) {
    if (Platform.OS !== 'android') return;
    try {
      const PomodoroService = require('../../modules/pomodoro-service');
      if (PomodoroService?.isAvailable()) {
        PomodoroService.startTimer(seconds, '', timerPhase);
      }
    } catch {}
  }

  const start = useCallback(async (title: string, onSessionComplete?: (session: number, totalMinutes: number) => void) => {
    const saved = await loadPomodoroSettings();
    const s = saved || DEFAULT_POMODORO;
    setSettings(s);
    setPhase('work');
    setSecondsLeft(s.workMinutes * 60);
    setRunning(true);
    setCurrentSession(1);
    setCompletedSessions(0);
    setItemTitle(title);
    setActive(true);
    setModalVisible(true);
    onSessionCompleteRef.current = onSessionComplete;
    syncNativeStart(s.workMinutes * 60, title, 'work');
  }, []);

  const pause = useCallback(() => {
    setRunning(false);
    syncNativePause();
  }, []);

  const resume = useCallback(() => {
    setRunning(true);
    syncNativeResume();
  }, []);

  const stop = useCallback(() => {
    setRunning(false);
    setActive(false);
    setModalVisible(false);
    setPhase('work');
    setSecondsLeft(settingsRef.current.workMinutes * 60);
    setCurrentSession(1);
    setCompletedSessions(0);
    onSessionCompleteRef.current = undefined;
    syncNativeStop();
  }, []);

  const openModal = useCallback(() => {
    setModalVisible(true);
  }, []);

  const closeModal = useCallback(() => {
    setModalVisible(false);
  }, []);

  const value: PomodoroContextType = {
    phase,
    secondsLeft,
    running,
    currentSession,
    completedSessions,
    settings,
    itemTitle,
    active,
    modalVisible,
    start,
    pause,
    resume,
    stop,
    openModal,
    closeModal,
  };

  return <PomodoroContext.Provider value={value}>{children}</PomodoroContext.Provider>;
}

export function usePomodoro(): PomodoroContextType {
  const context = useContext(PomodoroContext);
  if (context === undefined) {
    throw new Error('usePomodoro must be used within a PomodoroProvider');
  }
  return context;
}
