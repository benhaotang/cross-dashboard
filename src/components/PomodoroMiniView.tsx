import React from 'react';
import { View, Text, StyleSheet, TouchableOpacity, Platform } from 'react-native';
import { useTheme } from '../hooks/useTheme';
import { usePomodoro } from '../store/PomodoroContext';
import AppIcon, { Icons } from './Icon';

export default function PomodoroMiniView() {
  const theme = useTheme();
  const c = theme.colors;
  const pomodoro = usePomodoro();

  // Only show on desktop platforms when timer is active and modal is not visible
  if (Platform.OS !== 'web' && Platform.OS !== 'macos') return null;
  if (!pomodoro.active || pomodoro.modalVisible) return null;

  const { phase, secondsLeft, running, itemTitle, pause, resume, stop, openModal } = pomodoro;

  const minutes = Math.floor(secondsLeft / 60);
  const seconds = secondsLeft % 60;
  const timeDisplay = `${String(minutes).padStart(2, '0')}:${String(seconds).padStart(2, '0')}`;

  const phaseLabel = phase === 'work' ? 'Work' : phase === 'shortBreak' ? 'Short Break' : 'Long Break';
  const phaseColor = phase === 'work' ? c.primary : phase === 'shortBreak' ? '#4CAF50' : '#FF9800';

  return (
    <View style={[styles.container, { backgroundColor: c.surface, borderColor: c.border }]}>
      {/* Phase color bar */}
      <View style={[styles.phaseBar, { backgroundColor: phaseColor }]} />

      <View style={styles.body}>
        {/* Task name */}
        <Text style={[styles.taskName, { color: c.textSecondary }]} numberOfLines={1}>
          {itemTitle}
        </Text>

        {/* Countdown */}
        <Text style={[styles.countdown, { color: c.text }]}>{timeDisplay}</Text>

        {/* Phase label */}
        <Text style={[styles.phaseLabel, { color: phaseColor }]}>{phaseLabel}</Text>

        {/* Controls */}
        <View style={styles.controlRow}>
          <TouchableOpacity onPress={() => running ? pause() : resume()} style={styles.controlButton}>
            <AppIcon name={running ? Icons.pause : Icons.play} size={24} color={phaseColor} />
          </TouchableOpacity>
          <TouchableOpacity onPress={stop} style={styles.controlButton}>
            <AppIcon name={Icons.stop} size={24} color={c.textSecondary} />
          </TouchableOpacity>
          <TouchableOpacity onPress={openModal} style={styles.controlButton}>
            <AppIcon name={Icons.timer} size={24} color={c.textSecondary} />
          </TouchableOpacity>
        </View>
      </View>
    </View>
  );
}

const styles = StyleSheet.create({
  container: {
    position: 'absolute',
    bottom: 24,
    right: 24,
    width: 220,
    borderRadius: 12,
    borderWidth: 1,
    overflow: 'hidden',
    ...(Platform.OS === 'web'
      ? { boxShadow: '0 4px 12px rgba(0,0,0,0.15)' }
      : {}),
    zIndex: 9999,
  },
  phaseBar: {
    height: 4,
  },
  body: {
    padding: 12,
    alignItems: 'center',
  },
  taskName: {
    fontSize: 12,
    marginBottom: 4,
  },
  countdown: {
    fontSize: 28,
    fontWeight: '300',
    fontVariant: ['tabular-nums'],
    marginBottom: 2,
  },
  phaseLabel: {
    fontSize: 11,
    fontWeight: '700',
    textTransform: 'uppercase',
    marginBottom: 8,
  },
  controlRow: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 16,
  },
  controlButton: {
    padding: 4,
  },
});
