import React, { useState } from 'react';
import { View, Text, TouchableOpacity, StyleSheet, useWindowDimensions } from 'react-native';
import { NavigationContainer } from '@react-navigation/native';
import {
  DashboardScreen,
  InboxScreen,
  EventsScreen,
  NotesScreen,
  IssuesScreen,
  SettingsScreen,
} from '../screens';
import AppIcon, { Icons } from '../components/Icon';

type Screen = 'Dashboard' | 'Inbox' | 'Events' | 'Notes' | 'Issues' | 'Settings';

interface NavItem {
  name: Screen;
  icon: string;
  component: React.ComponentType;
}

const navItems: NavItem[] = [
  { name: 'Dashboard', icon: Icons.dashboard, component: DashboardScreen },
  { name: 'Inbox', icon: Icons.inbox, component: InboxScreen },
  { name: 'Events', icon: Icons.calendar, component: EventsScreen },
  { name: 'Notes', icon: Icons.notes, component: NotesScreen },
  { name: 'Issues', icon: Icons.issues, component: IssuesScreen },
  { name: 'Settings', icon: Icons.settings, component: SettingsScreen },
];

export default function SidebarNavigator() {
  const [activeScreen, setActiveScreen] = useState<Screen>('Dashboard');
  const { width } = useWindowDimensions();
  const isCollapsed = width < 900;

  const ActiveComponent = navItems.find((item) => item.name === activeScreen)?.component || DashboardScreen;

  return (
    <NavigationContainer>
      <View style={styles.container}>
        <View style={[styles.sidebar, isCollapsed && styles.sidebarCollapsed]}>
          <View style={styles.logo}>
            <AppIcon name={Icons.dashboard} size={28} color="#007AFF" />
            {!isCollapsed && <Text style={styles.logoText}>Cross Dashboard</Text>}
          </View>

          <View style={styles.navItems}>
            {navItems.map((item) => (
              <TouchableOpacity
                key={item.name}
                style={[
                  styles.navItem,
                  activeScreen === item.name && styles.navItemActive,
                  isCollapsed && styles.navItemCollapsed,
                ]}
                onPress={() => setActiveScreen(item.name)}
              >
                <AppIcon
                  name={item.icon}
                  size={22}
                  color={activeScreen === item.name ? '#007AFF' : '#666'}
                />
                {!isCollapsed && (
                  <Text
                    style={[
                      styles.navItemText,
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
    backgroundColor: '#f5f5f5',
  },
  sidebar: {
    width: 240,
    backgroundColor: '#fff',
    borderRightWidth: 1,
    borderRightColor: '#e0e0e0',
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
    color: '#333',
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
  navItemActive: {
    backgroundColor: '#E3F2FD',
  },
  navItemText: {
    fontSize: 15,
    color: '#666',
    fontWeight: '500',
  },
  navItemTextActive: {
    color: '#007AFF',
    fontWeight: '600',
  },
  content: {
    flex: 1,
  },
});
