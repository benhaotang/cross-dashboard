import React from 'react';
import { Platform } from 'react-native';
import { NavigationContainer } from '@react-navigation/native';
import { createBottomTabNavigator } from '@react-navigation/bottom-tabs';
import {
  DashboardScreen,
  InboxScreen,
  EventsScreen,
  NotesScreen,
  IssuesScreen,
  SettingsScreen,
} from '../screens';
import AppIcon, { Icons } from '../components/Icon';
import SidebarNavigator from './SidebarNavigator';

const Tab = createBottomTabNavigator();

const tabIcons: Record<string, string> = {
  Dashboard: Icons.dashboard,
  Inbox: Icons.inbox,
  Events: Icons.calendar,
  Notes: Icons.notes,
  Issues: Icons.issues,
  Settings: Icons.settings,
};

function MobileNavigator() {
  return (
    <NavigationContainer>
      <Tab.Navigator
        screenOptions={({ route }) => ({
          tabBarIcon: ({ focused }) => (
            <AppIcon
              name={tabIcons[route.name] || Icons.dashboard}
              size={24}
              color={focused ? '#007AFF' : '#999'}
            />
          ),
          tabBarActiveTintColor: '#007AFF',
          tabBarInactiveTintColor: '#999',
          headerShown: false,
        })}
      >
        <Tab.Screen name="Dashboard" component={DashboardScreen} />
        <Tab.Screen name="Inbox" component={InboxScreen} />
        <Tab.Screen name="Events" component={EventsScreen} />
        <Tab.Screen name="Notes" component={NotesScreen} />
        <Tab.Screen name="Issues" component={IssuesScreen} />
        <Tab.Screen name="Settings" component={SettingsScreen} />
      </Tab.Navigator>
    </NavigationContainer>
  );
}

export default function AppNavigator() {
  // Use sidebar navigation for web and macOS, bottom tabs for mobile
  if (Platform.OS === 'web' || Platform.OS === 'macos') {
    return <SidebarNavigator />;
  }
  return <MobileNavigator />;
}
