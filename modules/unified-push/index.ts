import { Platform } from 'react-native';
import { requireNativeModule } from 'expo-modules-core';

export interface EndpointEvent {
  endpoint: string;
  instance: string;
}

export interface MessageEvent {
  message: string;
  instance: string;
}

export interface InstanceEvent {
  instance: string;
}

interface RemovableSubscription {
  remove(): void;
}

interface UPNativeModule {
  register(instance: string): boolean;
  unregister(instance: string): boolean;
  getDistributors(): string[];
  saveDistributor(distributor: string): boolean;
  getDistributor(): string;
  addListener(eventName: string, listener: (...args: unknown[]) => void): RemovableSubscription;
  removeAllListeners(eventName: string): void;
}

// Only load the native module on Android
let nativeModule: UPNativeModule | null = null;
if (Platform.OS === 'android') {
  try {
    nativeModule = requireNativeModule<UPNativeModule>('UnifiedPush');
  } catch {
    // Module not available (e.g. in Expo Go)
  }
}

export function isAvailable(): boolean {
  return nativeModule !== null;
}

export function getDistributors(): string[] {
  if (!nativeModule) return [];
  return nativeModule.getDistributors();
}

export function saveDistributor(distributor: string): boolean {
  if (!nativeModule) return false;
  return nativeModule.saveDistributor(distributor);
}

export function getDistributor(): string {
  if (!nativeModule) return '';
  return nativeModule.getDistributor();
}

export function register(instance: string = 'default'): boolean {
  if (!nativeModule) return false;
  return nativeModule.register(instance);
}

export function unregister(instance: string = 'default'): boolean {
  if (!nativeModule) return false;
  return nativeModule.unregister(instance);
}

export function addEndpointListener(listener: (event: EndpointEvent) => void): RemovableSubscription | null {
  if (!nativeModule) return null;
  return nativeModule.addListener('onNewEndpoint', listener as (...args: unknown[]) => void);
}

export function addMessageListener(listener: (event: MessageEvent) => void): RemovableSubscription | null {
  if (!nativeModule) return null;
  return nativeModule.addListener('onMessage', listener as (...args: unknown[]) => void);
}

export function addUnregisteredListener(listener: (event: InstanceEvent) => void): RemovableSubscription | null {
  if (!nativeModule) return null;
  return nativeModule.addListener('onUnregistered', listener as (...args: unknown[]) => void);
}

export function addRegistrationFailedListener(listener: (event: InstanceEvent) => void): RemovableSubscription | null {
  if (!nativeModule) return null;
  return nativeModule.addListener('onRegistrationFailed', listener as (...args: unknown[]) => void);
}
