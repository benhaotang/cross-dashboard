import React, { useEffect, useState } from 'react';
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
import * as cache from '../services/cache';
import { StatsStore, DailyStats } from '../services/cache';
import AppIcon, { Icons } from '../components/Icon';

export default function DashboardScreen() {
  const { state } = useApp();
  const theme = useTheme();
  const { syncAll } = useSyncAll();
  const [statsStore, setStatsStore] = useState<StatsStore>({});

  useEffect(() => {
    cache.loadStatsStore().then(setStatsStore);
  }, [state.lastSync]);

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

  const this7: DailyStats = cache.sumStatRange(statsStore, 0, 7);
  const prev7: DailyStats = cache.sumStatRange(statsStore, 7, 7);
  const hasPrev = prev7.tasksCompleted + prev7.pomodoroSessions + prev7.issuesClosed > 0;

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

      {/* Activity Stats — last 7 days */}
      {(state.caldavConfigured || state.giteaConfigured) && (
        <View style={[styles.statsCard, { backgroundColor: c.surface }]}>
          <Text style={[styles.statsTitle, { color: c.text }]}>Last 7 Days</Text>
          <View style={styles.statsRow}>
            {/* Tasks */}
            <View style={[styles.statTile, { backgroundColor: c.background }]}>
              <AppIcon name={Icons.task} size={18} color="#4CAF50" />
              <Text style={[styles.statValue, { color: c.text }]}>{this7.tasksCompleted}</Text>
              <Text style={[styles.statLabel, { color: c.textSecondary }]}>tasks done</Text>
              {hasPrev && (
                <Text style={[styles.statDelta, {
                  color: this7.tasksCompleted > prev7.tasksCompleted ? '#4CAF50'
                    : this7.tasksCompleted < prev7.tasksCompleted ? '#FF9800' : c.textTertiary,
                }]}>
                  {this7.tasksCompleted > prev7.tasksCompleted ? `↑${this7.tasksCompleted - prev7.tasksCompleted}`
                    : this7.tasksCompleted < prev7.tasksCompleted ? `↓${prev7.tasksCompleted - this7.tasksCompleted}`
                    : '–'}
                </Text>
              )}
            </View>

            {/* Pomodoro */}
            <View style={[styles.statTile, { backgroundColor: c.background }]}>
              <AppIcon name={Icons.timer} size={18} color="#FF5722" />
              <Text style={[styles.statValue, { color: c.text }]}>{this7.pomodoroSessions}</Text>
              <Text style={[styles.statLabel, { color: c.textSecondary }]}>pomodoros</Text>
              {hasPrev && (
                <Text style={[styles.statDelta, {
                  color: this7.pomodoroSessions > prev7.pomodoroSessions ? '#4CAF50'
                    : this7.pomodoroSessions < prev7.pomodoroSessions ? '#FF9800' : c.textTertiary,
                }]}>
                  {this7.pomodoroSessions > prev7.pomodoroSessions ? `↑${this7.pomodoroSessions - prev7.pomodoroSessions}`
                    : this7.pomodoroSessions < prev7.pomodoroSessions ? `↓${prev7.pomodoroSessions - this7.pomodoroSessions}`
                    : '–'}
                </Text>
              )}
            </View>

            {/* Issues */}
            {state.giteaConfigured && (
              <View style={[styles.statTile, { backgroundColor: c.background }]}>
                <AppIcon name={Icons.issues} size={18} color="#9C27B0" />
                <Text style={[styles.statValue, { color: c.text }]}>{this7.issuesClosed}</Text>
                <Text style={[styles.statLabel, { color: c.textSecondary }]}>issues closed</Text>
                {hasPrev && (
                  <Text style={[styles.statDelta, {
                    color: this7.issuesClosed > prev7.issuesClosed ? '#4CAF50'
                      : this7.issuesClosed < prev7.issuesClosed ? '#FF9800' : c.textTertiary,
                  }]}>
                    {this7.issuesClosed > prev7.issuesClosed ? `↑${this7.issuesClosed - prev7.issuesClosed}`
                      : this7.issuesClosed < prev7.issuesClosed ? `↓${prev7.issuesClosed - this7.issuesClosed}`
                      : '–'}
                  </Text>
                )}
              </View>
            )}
          </View>
          {hasPrev && (
            <Text style={[styles.statCaption, { color: c.textTertiary }]}>vs previous 7 days</Text>
          )}
        </View>
      )}

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
  statsCard: {
    marginHorizontal: 16,
    marginTop: 16,
    padding: 14,
    borderRadius: 8,
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 1 },
    shadowOpacity: 0.1,
    shadowRadius: 2,
    elevation: 2,
  },
  statsTitle: {
    fontSize: 14,
    fontWeight: '700',
    textTransform: 'uppercase',
    letterSpacing: 0.5,
    marginBottom: 10,
  },
  statsRow: {
    flexDirection: 'row',
    gap: 8,
  },
  statTile: {
    flex: 1,
    alignItems: 'center',
    paddingVertical: 10,
    paddingHorizontal: 4,
    borderRadius: 8,
    gap: 2,
  },
  statValue: {
    fontSize: 26,
    fontWeight: '700',
    lineHeight: 30,
  },
  statLabel: {
    fontSize: 10,
    fontWeight: '500',
    textAlign: 'center',
  },
  statDelta: {
    fontSize: 11,
    fontWeight: '600',
    marginTop: 2,
  },
  statCaption: {
    fontSize: 10,
    textAlign: 'right',
    marginTop: 6,
  },
});
