import React from 'react';
import {
  View,
  Text,
  StyleSheet,
  ScrollView,
  TouchableOpacity,
  ActivityIndicator,
} from 'react-native';
import { useApp } from '../store/AppContext';
import { useTheme } from '../hooks/useTheme';
import { useSyncAll } from '../hooks/useSyncAll';
import AppIcon, { Icons } from '../components/Icon';

export default function DashboardScreen() {
  const { state } = useApp();
  const theme = useTheme();
  const { syncAll } = useSyncAll();

  const upcomingEvents = state.events
    .filter((e) => e.start >= new Date())
    .sort((a, b) => a.start.getTime() - b.start.getTime())
    .slice(0, 5);

  const upcomingTasks = state.tasks
    .filter((t) => t.status !== 'COMPLETED' && t.status !== 'CANCELLED')
    .sort((a, b) => {
      const aDate = a.due?.getTime() ?? Infinity;
      const bDate = b.due?.getTime() ?? Infinity;
      return aDate - bDate;
    })
    .slice(0, 5);

  const openIssues = state.issues.filter((i) => i.state === 'open').slice(0, 5);

  const c = theme.colors;

  return (
    <ScrollView style={[styles.container, { backgroundColor: c.background }]}>
      <View style={[styles.header, { backgroundColor: c.surface, borderBottomColor: c.border }]}>
        <View>
          <Text style={[styles.title, { color: c.text }]}>Dashboard</Text>
          {state.lastSync && (
            <Text style={[styles.lastSync, { color: c.textTertiary }]}>
              Synced {state.lastSync.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' })}
            </Text>
          )}
        </View>
        <TouchableOpacity style={[styles.refreshButton, { backgroundColor: c.primary }]} onPress={syncAll}>
          <AppIcon name={Icons.refresh} size={18} color="#fff" />
          <Text style={styles.refreshText}>Refresh</Text>
        </TouchableOpacity>
      </View>

      {state.isLoading && <ActivityIndicator size="large" color={c.primary} style={styles.loader} />}

      {!state.caldavConfigured && !state.giteaConfigured && (
        <View style={[styles.card, { backgroundColor: c.surface }]}>
          <Text style={[styles.cardTitle, { color: c.text }]}>Welcome!</Text>
          <Text style={[styles.cardText, { color: c.textSecondary }]}>
            Configure your CalDAV and Gitea credentials in Settings to get started.
          </Text>
        </View>
      )}

      <View style={styles.section}>
        <Text style={[styles.sectionTitle, { color: c.text }]}>Upcoming Events</Text>
        {!state.caldavConfigured ? (
          <Text style={[styles.emptyText, { color: c.textTertiary }]}>CalDAV not configured</Text>
        ) : upcomingEvents.length === 0 ? (
          <Text style={[styles.emptyText, { color: c.textTertiary }]}>No upcoming events</Text>
        ) : (
          upcomingEvents.map((event) => (
            <View key={event.uid} style={[styles.card, { backgroundColor: c.surface }]}>
              <Text style={[styles.cardTitle, { color: c.text }]}>{event.summary}</Text>
              <Text style={[styles.cardText, { color: c.textSecondary }]}>
                {event.start.toLocaleDateString()} {event.start.toLocaleTimeString()}
              </Text>
              {event.location && <Text style={[styles.cardSubtext, { color: c.textTertiary }]}>{event.location}</Text>}
            </View>
          ))
        )}
      </View>

      <View style={styles.section}>
        <Text style={[styles.sectionTitle, { color: c.text }]}>Tasks Due Soon</Text>
        {!state.caldavConfigured ? (
          <Text style={[styles.emptyText, { color: c.textTertiary }]}>CalDAV not configured</Text>
        ) : upcomingTasks.length === 0 ? (
          <Text style={[styles.emptyText, { color: c.textTertiary }]}>No pending tasks</Text>
        ) : (
          upcomingTasks.map((task) => (
            <View key={task.uid} style={[styles.card, { backgroundColor: c.surface }]}>
              <View style={{ flexDirection: 'row', alignItems: 'center', gap: 8 }}>
                <AppIcon name={Icons.taskOutline} size={16} color={task.priority >= 1 && task.priority <= 4 ? '#F44336' : '#FF9800'} />
                <Text style={[styles.cardTitle, { color: c.text, marginBottom: 0 }]}>{task.summary}</Text>
              </View>
              {task.due && (
                <Text style={[styles.cardText, { color: task.due < new Date() ? '#F44336' : c.textSecondary }]}>
                  Due: {task.due.toLocaleDateString()}
                </Text>
              )}
              {task.percentComplete > 0 && (
                <Text style={[styles.cardSubtext, { color: c.textTertiary }]}>{task.percentComplete}% complete</Text>
              )}
            </View>
          ))
        )}
      </View>

      <View style={styles.section}>
        <Text style={[styles.sectionTitle, { color: c.text }]}>Open Issues</Text>
        {!state.giteaConfigured ? (
          <Text style={[styles.emptyText, { color: c.textTertiary }]}>Gitea not configured</Text>
        ) : openIssues.length === 0 ? (
          <Text style={[styles.emptyText, { color: c.textTertiary }]}>No open issues</Text>
        ) : (
          openIssues.map((issue) => (
            <View key={issue.id} style={[styles.card, { backgroundColor: c.surface }]}>
              <Text style={[styles.cardTitle, { color: c.text }]}>
                #{issue.number} {issue.title}
              </Text>
              <Text style={[styles.cardSubtext, { color: c.textTertiary }]}>{issue.repository}</Text>
              <View style={styles.labelContainer}>
                {issue.labels.map((label) => (
                  <View
                    key={label.id}
                    style={[styles.label, { backgroundColor: `#${label.color}` }]}
                  >
                    <Text style={styles.labelText}>{label.name}</Text>
                  </View>
                ))}
              </View>
            </View>
          ))
        )}
      </View>
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
  },
  header: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    padding: 16,
    borderBottomWidth: 1,
  },
  title: {
    fontSize: 24,
    fontWeight: 'bold',
  },
  lastSync: {
    fontSize: 12,
    marginTop: 2,
  },
  refreshButton: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 6,
    paddingHorizontal: 16,
    paddingVertical: 8,
    borderRadius: 8,
  },
  refreshText: {
    color: '#fff',
    fontWeight: '600',
  },
  loader: {
    marginTop: 20,
  },
  section: {
    padding: 16,
  },
  sectionTitle: {
    fontSize: 18,
    fontWeight: '600',
    marginBottom: 12,
  },
  card: {
    padding: 16,
    borderRadius: 8,
    marginBottom: 8,
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 1 },
    shadowOpacity: 0.1,
    shadowRadius: 2,
    elevation: 2,
  },
  cardTitle: {
    fontSize: 16,
    fontWeight: '600',
    marginBottom: 4,
  },
  cardText: {
    fontSize: 14,
  },
  cardSubtext: {
    fontSize: 12,
    marginTop: 4,
  },
  emptyText: {
    fontStyle: 'italic',
  },
  labelContainer: {
    flexDirection: 'row',
    flexWrap: 'wrap',
    marginTop: 8,
  },
  label: {
    paddingHorizontal: 8,
    paddingVertical: 2,
    borderRadius: 12,
    marginRight: 4,
    marginBottom: 4,
  },
  labelText: {
    fontSize: 10,
    color: '#fff',
    fontWeight: '600',
  },
});
