import React, { useEffect } from 'react';
import {
  View,
  Text,
  StyleSheet,
  ScrollView,
  TouchableOpacity,
  ActivityIndicator,
} from 'react-native';
import { useApp } from '../store/AppContext';
import * as caldav from '../services/caldav';
import * as gitea from '../services/gitea';
import AppIcon, { Icons } from '../components/Icon';

export default function DashboardScreen() {
  const { state, setEvents, setIssues, setLoading, setCaldavConfigured, setGiteaConfigured } =
    useApp();

  useEffect(() => {
    checkConfiguration();
  }, []);

  async function checkConfiguration() {
    const [caldavOk, giteaOk] = await Promise.all([caldav.isConfigured(), gitea.isConfigured()]);
    setCaldavConfigured(caldavOk);
    setGiteaConfigured(giteaOk);
  }

  async function refreshData() {
    setLoading(true);
    try {
      if (state.caldavConfigured) {
        const events = await caldav.fetchEvents();
        setEvents(events);
      }
      if (state.giteaConfigured && state.giteaRepositories.length > 0) {
        const issues = await gitea.fetchAllIssues(state.giteaRepositories);
        setIssues(issues);
      }
    } catch (error) {
      console.error('Error refreshing data:', error);
    } finally {
      setLoading(false);
    }
  }

  const upcomingEvents = state.events
    .filter((e) => e.start >= new Date())
    .sort((a, b) => a.start.getTime() - b.start.getTime())
    .slice(0, 5);

  const openIssues = state.issues.filter((i) => i.state === 'open').slice(0, 5);

  return (
    <ScrollView style={styles.container}>
      <View style={styles.header}>
        <Text style={styles.title}>Dashboard</Text>
        <TouchableOpacity style={styles.refreshButton} onPress={refreshData}>
          <AppIcon name={Icons.refresh} size={18} color="#fff" />
          <Text style={styles.refreshText}>Refresh</Text>
        </TouchableOpacity>
      </View>

      {state.isLoading && <ActivityIndicator size="large" color="#007AFF" />}

      {!state.caldavConfigured && !state.giteaConfigured && (
        <View style={styles.card}>
          <Text style={styles.cardTitle}>Welcome!</Text>
          <Text style={styles.cardText}>
            Configure your CalDAV and Gitea credentials in Settings to get started.
          </Text>
        </View>
      )}

      <View style={styles.section}>
        <Text style={styles.sectionTitle}>Upcoming Events</Text>
        {!state.caldavConfigured ? (
          <Text style={styles.emptyText}>CalDAV not configured</Text>
        ) : upcomingEvents.length === 0 ? (
          <Text style={styles.emptyText}>No upcoming events</Text>
        ) : (
          upcomingEvents.map((event) => (
            <View key={event.uid} style={styles.card}>
              <Text style={styles.cardTitle}>{event.summary}</Text>
              <Text style={styles.cardText}>
                {event.start.toLocaleDateString()} {event.start.toLocaleTimeString()}
              </Text>
              {event.location && <Text style={styles.cardSubtext}>{event.location}</Text>}
            </View>
          ))
        )}
      </View>

      <View style={styles.section}>
        <Text style={styles.sectionTitle}>Open Issues</Text>
        {!state.giteaConfigured ? (
          <Text style={styles.emptyText}>Gitea not configured</Text>
        ) : openIssues.length === 0 ? (
          <Text style={styles.emptyText}>No open issues</Text>
        ) : (
          openIssues.map((issue) => (
            <View key={issue.id} style={styles.card}>
              <Text style={styles.cardTitle}>
                #{issue.number} {issue.title}
              </Text>
              <Text style={styles.cardSubtext}>{issue.repository}</Text>
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
    backgroundColor: '#f5f5f5',
  },
  header: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    padding: 16,
    backgroundColor: '#fff',
    borderBottomWidth: 1,
    borderBottomColor: '#e0e0e0',
  },
  title: {
    fontSize: 24,
    fontWeight: 'bold',
  },
  refreshButton: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 6,
    backgroundColor: '#007AFF',
    paddingHorizontal: 16,
    paddingVertical: 8,
    borderRadius: 8,
  },
  refreshText: {
    color: '#fff',
    fontWeight: '600',
  },
  section: {
    padding: 16,
  },
  sectionTitle: {
    fontSize: 18,
    fontWeight: '600',
    marginBottom: 12,
    color: '#333',
  },
  card: {
    backgroundColor: '#fff',
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
    color: '#666',
  },
  cardSubtext: {
    fontSize: 12,
    color: '#999',
    marginTop: 4,
  },
  emptyText: {
    color: '#999',
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
