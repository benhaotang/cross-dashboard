import React from 'react';
import {
  View,
  Text,
  StyleSheet,
  Modal,
  TouchableOpacity,
} from 'react-native';
import { useTheme } from '../hooks/useTheme';
import { usePomodoro } from '../store/PomodoroContext';
import AppIcon, { Icons } from './Icon';

export default function PomodoroTimer() {
  const theme = useTheme();
  const c = theme.colors;
  const pomodoro = usePomodoro();

  const {
    modalVisible,
    phase,
    secondsLeft,
    running,
    currentSession,
    completedSessions,
    settings,
    itemTitle,
    closeModal,
    stop,
    pause,
    resume,
  } = pomodoro;

  const minutes = Math.floor(secondsLeft / 60);
  const seconds = secondsLeft % 60;
  const timeDisplay = `${String(minutes).padStart(2, '0')}:${String(seconds).padStart(2, '0')}`;

  const phaseLabel = phase === 'work' ? 'Work' : phase === 'shortBreak' ? 'Short Break' : 'Long Break';
  const phaseColor = phase === 'work' ? c.primary : phase === 'shortBreak' ? '#4CAF50' : '#FF9800';

  const totalDots = settings.sessionsUntilLongBreak;
  const dots = Array.from({ length: totalDots }, (_, i) => i < completedSessions);

  function handleClose() {
    closeModal();
  }

  function handleReset() {
    stop();
  }

  return (
    <Modal visible={modalVisible} animationType="slide" transparent>
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
              <TouchableOpacity onPress={() => running ? pause() : resume()} style={styles.controlButton}>
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
