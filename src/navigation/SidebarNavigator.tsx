import React, { useState, useMemo } from 'react';
import { View, Text, TouchableOpacity, StyleSheet, useWindowDimensions } from 'react-native';
import { NavigationContainer } from '@react-navigation/native';
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
import { useTheme } from '../hooks/useTheme';
import { useApp } from '../store/AppContext';

type Screen = 'Dashboard' | 'Inbox' | 'Events' | 'Notes' | 'Tasks' | 'Issues' | 'Views' | 'Settings';

interface NavItem {
  name: Screen;
  icon: string;
  component: React.ComponentType;
}

const allNavItems: NavItem[] = [
  { name: 'Dashboard', icon: Icons.dashboard, component: DashboardScreen },
  { name: 'Inbox', icon: Icons.inbox, component: InboxScreen },
  { name: 'Events', icon: Icons.calendar, component: EventsScreen },
  { name: 'Notes', icon: Icons.notes, component: NotesScreen },
  { name: 'Tasks', icon: Icons.task, component: TasksScreen },
  { name: 'Issues', icon: Icons.issues, component: IssuesScreen },
  { name: 'Views', icon: Icons.views, component: ViewsScreen },
  { name: 'Settings', icon: Icons.settings, component: SettingsScreen },
];

export default function SidebarNavigator() {
  const [activeScreen, setActiveScreen] = useState<Screen>('Dashboard');
  const { width } = useWindowDimensions();
  const isCollapsed = width < 900;
  const theme = useTheme();
  const { state } = useApp();

  const navItems = useMemo(() => {
    const visible = new Set(state.visibleScreens);
    return allNavItems.filter((item) => item.name === 'Settings' || visible.has(item.name));
  }, [state.visibleScreens]);

  const ActiveComponent = navItems.find((item) => item.name === activeScreen)?.component || navItems[0]?.component || DashboardScreen;

  return (
    <NavigationContainer documentTitle={{ formatter: () => 'Cross Dashboard' }}>
      <View style={[styles.container, { backgroundColor: theme.colors.background }]}>
        <View style={[
          styles.sidebar,
          isCollapsed && styles.sidebarCollapsed,
          { backgroundColor: theme.colors.surface, borderRightColor: theme.colors.border },
        ]}>
          <View style={styles.logo}>
            <AppIcon name={Icons.dashboard} size={28} color={theme.colors.primary} />
            {!isCollapsed && <Text style={[styles.logoText, { color: theme.colors.text }]}>Cross Dashboard</Text>}
          </View>

          <View style={styles.navItems}>
            {navItems.map((item) => (
              <TouchableOpacity
                key={item.name}
                style={[
                  styles.navItem,
                  activeScreen === item.name && { backgroundColor: theme.colors.sidebarActiveBg },
                  isCollapsed && styles.navItemCollapsed,
                ]}
                onPress={() => setActiveScreen(item.name)}
              >
                <AppIcon
                  name={item.icon}
                  size={22}
                  color={activeScreen === item.name ? theme.colors.primary : theme.colors.textSecondary}
                />
                {!isCollapsed && (
                  <Text
                    style={[
                      styles.navItemText,
                      { color: activeScreen === item.name ? theme.colors.primary : theme.colors.textSecondary },
                      activeScreen === item.name && styles.navItemTextActive,
                    ]}
                  >
                    {item.name}
                  </Text>
                )}
              </TouchableOpacity>
            ))}
          </View>
        </View>

        <View style={styles.content}>
          <ActiveComponent />
        </View>
      </View>
    </NavigationContainer>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    flexDirection: 'row',
  },
  sidebar: {
    width: 240,
    borderRightWidth: 1,
    paddingVertical: 16,
  },
  sidebarCollapsed: {
    width: 68,
  },
  logo: {
    flexDirection: 'row',
    alignItems: 'center',
    paddingHorizontal: 16,
    paddingVertical: 12,
    marginBottom: 16,
    gap: 12,
  },
  logoText: {
    fontSize: 18,
    fontWeight: '700',
  },
  navItems: {
    flex: 1,
  },
  navItem: {
    flexDirection: 'row',
    alignItems: 'center',
    paddingVertical: 12,
    paddingHorizontal: 16,
    marginHorizontal: 8,
    marginVertical: 2,
    borderRadius: 8,
    gap: 12,
  },
  navItemCollapsed: {
    justifyContent: 'center',
    paddingHorizontal: 0,
  },
  navItemText: {
    fontSize: 15,
    fontWeight: '500',
  },
  navItemTextActive: {
    fontWeight: '600',
  },
  content: {
    flex: 1,
  },
});
