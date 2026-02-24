import { Platform } from 'react-native';
import { requireNativeModule } from 'expo-modules-core';

export interface TickEvent {
  secondsLeft: number;
}

export interface PhaseEndEvent {
  phase: string;
}

export interface ActionEvent {
  action: 'pause' | 'resume' | 'stop';
}

interface RemovableSubscription {
  remove(): void;
}

interface PomodoroNativeModule {
  startTimer(seconds: number, taskName: string, phase: string): boolean;
  pauseTimer(): boolean;
  resumeTimer(): boolean;
  stopTimer(): boolean;
  canScheduleExactAlarms(): boolean;
  addListener(eventName: string, listener: (...args: unknown[]) => void): RemovableSubscription;
  removeAllListeners(eventName: string): void;
}

let nativeModule: PomodoroNativeModule | null = null;
if (Platform.OS === 'android') {
  try {
    nativeModule = requireNativeModule<PomodoroNativeModule>('PomodoroService');
  } catch {
    // Module not available (e.g. in Expo Go)
  }
}

export function isAvailable(): boolean {
  return nativeModule !== null;
}

export function startTimer(seconds: number, taskName: string, phase: string): boolean {
  if (!nativeModule) return false;
  return nativeModule.startTimer(seconds, taskName, phase);
}

export function pauseTimer(): boolean {
  if (!nativeModule) return false;
  return nativeModule.pauseTimer();
}

export function resumeTimer(): boolean {
  if (!nativeModule) return false;
  return nativeModule.resumeTimer();
}

export function stopTimer(): boolean {
  if (!nativeModule) return false;
  return nativeModule.stopTimer();
}

export function canScheduleExactAlarms(): boolean {
  if (!nativeModule) return false;
  return nativeModule.canScheduleExactAlarms();
}

export function addTickListener(listener: (event: TickEvent) => void): RemovableSubscription | null {
  if (!nativeModule) return null;
  return nativeModule.addListener('onTick', listener as (...args: unknown[]) => void);
}

export function addPhaseEndListener(listener: (event: PhaseEndEvent) => void): RemovableSubscription | null {
  if (!nativeModule) return null;
  return nativeModule.addListener('onPhaseEnd', listener as (...args: unknown[]) => void);
}

export function addActionListener(listener: (event: ActionEvent) => void): RemovableSubscription | null {
  if (!nativeModule) return null;
  return nativeModule.addListener('onAction', listener as (...args: unknown[]) => void);
}
