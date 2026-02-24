import React, { useState, useEffect, useRef, useCallback } from 'react';
import {
  View,
  Text,
  StyleSheet,
  Modal,
  TouchableOpacity,
} from 'react-native';
import { useTheme } from '../hooks/useTheme';
import { loadPomodoroSettings, DEFAULT_POMODORO, PomodoroSettings } from '../services/cache';
import AppIcon, { Icons } from './Icon';

interface Props {
  visible: boolean;
  onClose: () => void;
  itemTitle: string;
  onSessionComplete?: (sessionNumber: number, totalMinutes: number) => void;
}

type TimerPhase = 'work' | 'shortBreak' | 'longBreak';

export default function PomodoroTimer({ visible, onClose, itemTitle, onSessionComplete }: Props) {
  const theme = useTheme();
  const c = theme.colors;

  const [settings, setSettings] = useState<PomodoroSettings>(DEFAULT_POMODORO);
  const [phase, setPhase] = useState<TimerPhase>('work');
  const [secondsLeft, setSecondsLeft] = useState(DEFAULT_POMODORO.workMinutes * 60);
  const [running, setRunning] = useState(false);
  const [currentSession, setCurrentSession] = useState(1);
  const [completedSessions, setCompletedSessions] = useState(0);

  const intervalRef = useRef<ReturnType<typeof setInterval> | null>(null);
  const settingsRef = useRef(settings);
  const phaseRef = useRef(phase);
  const currentSessionRef = useRef(currentSession);
  const completedSessionsRef = useRef(completedSessions);
  const onSessionCompleteRef = useRef(onSessionComplete);

  settingsRef.current = settings;
  phaseRef.current = phase;
  currentSessionRef.current = currentSession;
  completedSessionsRef.current = completedSessions;
  onSessionCompleteRef.current = onSessionComplete;

  // Load settings on mount
  useEffect(() => {
    if (!visible) return;
    loadPomodoroSettings().then((saved) => {
      const s = saved || DEFAULT_POMODORO;
      setSettings(s);
      setPhase('work');
      setSecondsLeft(s.workMinutes * 60);
      setRunning(false);
      setCurrentSession(1);
      setCompletedSessions(0);
    });
    return () => {
      if (intervalRef.current) clearInterval(intervalRef.current);
    };
  }, [visible]);

  const handleTimerEnd = useCallback(() => {
    const s = settingsRef.current;
    if (phaseRef.current === 'work') {
      const newCompleted = completedSessionsRef.current + 1;
      setCompletedSessions(newCompleted);
      onSessionCompleteRef.current?.(newCompleted, s.workMinutes);

      // Determine break type
      const isLongBreak = newCompleted % s.sessionsUntilLongBreak === 0;
      if (isLongBreak) {
        setPhase('longBreak');
        setSecondsLeft(s.longBreakMinutes * 60);
      } else {
        setPhase('shortBreak');
        setSecondsLeft(s.shortBreakMinutes * 60);
      }
    } else {
      // Break ended -> next work session
      setCurrentSession(currentSessionRef.current + 1);
      setPhase('work');
      setSecondsLeft(s.workMinutes * 60);
    }
  }, []);

  // Interval tick
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

  function handleReset() {
    setRunning(false);
    setPhase('work');
    setSecondsLeft(settings.workMinutes * 60);
    setCurrentSession(1);
    setCompletedSessions(0);
  }

  function handleClose() {
    setRunning(false);
    if (intervalRef.current) clearInterval(intervalRef.current);
    onClose();
  }

  const minutes = Math.floor(secondsLeft / 60);
  const seconds = secondsLeft % 60;
  const timeDisplay = `${String(minutes).padStart(2, '0')}:${String(seconds).padStart(2, '0')}`;

  const phaseLabel = phase === 'work' ? 'Work' : phase === 'shortBreak' ? 'Short Break' : 'Long Break';
  const phaseColor = phase === 'work' ? c.primary : phase === 'shortBreak' ? '#4CAF50' : '#FF9800';

  // Session dots
  const totalDots = settings.sessionsUntilLongBreak;
  const dots = Array.from({ length: totalDots }, (_, i) => i < completedSessions);

  return (
    <Modal visible={visible} animationType="slide" transparent>
      <View style={styles.overlay}>
        <View style={[styles.container, { backgroundColor: c.surface }]}>
          {/* Header */}
          <View style={[styles.header, { borderBottomColor: c.border }]}>
            <TouchableOpacity onPress={handleClose} style={styles.headerButton}>
              <AppIcon name={Icons.close} size={22} color={c.text} />
            </TouchableOpacity>
            <Text style={[styles.headerTitle, { color: c.text }]} numberOfLines={1}>
              Pomodoro
            </Text>
            <TouchableOpacity onPress={handleReset} style={styles.headerButton}>
              <AppIcon name={Icons.stop} size={22} color={c.textSecondary} />
            </TouchableOpacity>
          </View>

          {/* Content */}
          <View style={styles.content}>
            {/* Item title */}
            <Text style={[styles.itemTitle, { color: c.textSecondary }]} numberOfLines={2}>
              {itemTitle}
            </Text>

            {/* Phase label */}
            <Text style={[styles.phaseLabel, { color: phaseColor }]}>{phaseLabel}</Text>

            {/* Countdown */}
            <Text style={[styles.countdown, { color: c.text }]}>{timeDisplay}</Text>

            {/* Session counter */}
            <Text style={[styles.sessionCounter, { color: c.textSecondary }]}>
              Session {currentSession} of {settings.sessionsUntilLongBreak}
            </Text>

            {/* Session dots */}
            <View style={styles.dotRow}>
              {dots.map((filled, i) => (
                <View
                  key={i}
                  style={[
                    styles.dot,
                    { backgroundColor: filled ? phaseColor : c.border },
                  ]}
                />
              ))}
            </View>

            {/* Controls */}
            <View style={styles.controlRow}>
              <TouchableOpacity onPress={() => setRunning(!running)} style={styles.controlButton}>
                <AppIcon
                  name={running ? Icons.pause : Icons.play}
                  size={64}
                  color={phaseColor}
                />
              </TouchableOpacity>
            </View>

            {/* Completed count */}
            {completedSessions > 0 && (
              <Text style={[styles.completedText, { color: c.textSecondary }]}>
                {completedSessions} session{completedSessions !== 1 ? 's' : ''} completed ({completedSessions * settings.workMinutes}min)
              </Text>
            )}
          </View>
        </View>
      </View>
    </Modal>
  );
}

const styles = StyleSheet.create({
  overlay: {
    flex: 1,
    backgroundColor: 'rgba(0,0,0,0.5)',
    justifyContent: 'flex-end',
  },
  container: {
    borderTopLeftRadius: 16,
    borderTopRightRadius: 16,
    maxHeight: '92%',
    minHeight: '50%',
  },
  header: {
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'space-between',
    paddingHorizontal: 16,
    paddingVertical: 14,
    borderBottomWidth: 1,
  },
  headerButton: {
    width: 60,
    alignItems: 'center',
  },
  headerTitle: {
    flex: 1,
    textAlign: 'center',
    fontSize: 17,
    fontWeight: '600',
  },
  content: {
    alignItems: 'center',
    paddingVertical: 32,
    paddingHorizontal: 20,
  },
  itemTitle: {
    fontSize: 15,
    textAlign: 'center',
    marginBottom: 16,
  },
  phaseLabel: {
    fontSize: 16,
    fontWeight: '700',
    textTransform: 'uppercase',
    marginBottom: 8,
  },
  countdown: {
    fontSize: 72,
    fontWeight: '200',
    fontVariant: ['tabular-nums'],
    marginBottom: 8,
  },
  sessionCounter: {
    fontSize: 14,
    marginBottom: 16,
  },
  dotRow: {
    flexDirection: 'row',
    gap: 8,
    marginBottom: 32,
  },
  dot: {
    width: 12,
    height: 12,
    borderRadius: 6,
  },
  controlRow: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 24,
    marginBottom: 24,
  },
  controlButton: {
    padding: 8,
  },
  completedText: {
    fontSize: 13,
  },
});
