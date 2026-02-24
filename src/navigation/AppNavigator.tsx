import React, { useMemo } from 'react';
import { Platform } from 'react-native';
import { NavigationContainer } from '@react-navigation/native';
import { createBottomTabNavigator } from '@react-navigation/bottom-tabs';
import {
  DashboardScreen,
  InboxScreen,
  EventsScreen,
  NotesScreen,
  TasksScreen,
  IssuesScreen,
  ViewsScreen,
  SettingsScreen,
} from '../screens';
import AppIcon, { Icons } from '../components/Icon';
import SidebarNavigator from './SidebarNavigator';
import { useTheme } from '../hooks/useTheme';
import { useApp } from '../store/AppContext';
import { ScreenName } from '../services/cache';

const Tab = createBottomTabNavigator();

const tabIcons: Record<string, string> = {
  Dashboard: Icons.dashboard,
  Inbox: Icons.inbox,
  Events: Icons.calendar,
  Notes: Icons.notes,
  Tasks: Icons.task,
  Issues: Icons.issues,
  Views: Icons.views,
  Settings: Icons.settings,
};

const allTabs: { name: string; component: React.ComponentType }[] = [
  { name: 'Dashboard', component: DashboardScreen },
  { name: 'Inbox', component: InboxScreen },
  { name: 'Events', component: EventsScreen },
  { name: 'Notes', component: NotesScreen },
  { name: 'Tasks', component: TasksScreen },
  { name: 'Issues', component: IssuesScreen },
  { name: 'Views', component: ViewsScreen },
  { name: 'Settings', component: SettingsScreen },
];

function MobileNavigator() {
  const theme = useTheme();
  const { state } = useApp();

  const visibleTabs = useMemo(() => {
    const visible = new Set(state.visibleScreens);
    return allTabs.filter((tab) => tab.name === 'Settings' || visible.has(tab.name as ScreenName));
  }, [state.visibleScreens]);

  return (
    <NavigationContainer>
      <Tab.Navigator
        screenOptions={({ route }) => ({
          tabBarIcon: ({ focused }) => (
            <AppIcon
              name={tabIcons[route.name] || Icons.dashboard}
              size={24}
              color={focused ? theme.colors.primary : theme.colors.textTertiary}
            />
          ),
          tabBarActiveTintColor: theme.colors.primary,
          tabBarInactiveTintColor: theme.colors.textTertiary,
          tabBarStyle: { backgroundColor: theme.colors.surface, borderTopColor: theme.colors.border },
          headerShown: false,
        })}
      >
        {visibleTabs.map((tab) => (
          <Tab.Screen key={tab.name} name={tab.name} component={tab.component} />
        ))}
      </Tab.Navigator>
    </NavigationContainer>
  );
}

export default function AppNavigator() {
  if (Platform.OS === 'web' || Platform.OS === 'macos') {
    return <SidebarNavigator />;
  }
  return <MobileNavigator />;
}
