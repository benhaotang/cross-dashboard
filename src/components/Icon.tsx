import React from 'react';
import { Platform } from 'react-native';

interface IconProps {
  name: string;
  size?: number;
  color?: string;
}

function toMciName(mdiName: string): string {
  return mdiName.replace(/^mdi:/, '');
}

// Web implementation using Iconify
function WebIcon({ name, size = 24, color = '#666' }: IconProps) {
  const { Icon } = require('@iconify/react');
  return <Icon icon={name} width={size} height={size} color={color} />;
}

// Native implementation using @expo/vector-icons MaterialCommunityIcons
function NativeIcon({ name, size = 24, color = '#666' }: IconProps) {
  const MaterialCommunityIcons = require('@expo/vector-icons/MaterialCommunityIcons').default;
  return <MaterialCommunityIcons name={toMciName(name)} size={size} color={color} />;
}

export default function AppIcon(props: IconProps) {
  if (Platform.OS === 'web') {
    return <WebIcon {...props} />;
  }
  return <NativeIcon {...props} />;
}

// Icon name constants for consistent usage
export const Icons = {
  dashboard: 'mdi:view-dashboard',
  inbox: 'mdi:inbox',
  calendar: 'mdi:calendar',
  notes: 'mdi:note-text',
  issues: 'mdi:bug',
  settings: 'mdi:cog',
  refresh: 'mdi:refresh',
  add: 'mdi:plus',
  delete: 'mdi:delete',
  close: 'mdi:close',
  check: 'mdi:check',
  chevronRight: 'mdi:chevron-right',
  menu: 'mdi:menu',
  search: 'mdi:magnify',
  filter: 'mdi:filter',
  location: 'mdi:map-marker',
  time: 'mdi:clock-outline',
  user: 'mdi:account',
  label: 'mdi:label',
  link: 'mdi:link',
  milestone: 'mdi:flag',
  task: 'mdi:checkbox-marked',
};
