import React, { useEffect, useState } from 'react';
import {
  View,
  Text,
  StyleSheet,
  FlatList,
  TouchableOpacity,
  ActivityIndicator,
} from 'react-native';
import { useApp } from '../store/AppContext';
import * as caldav from '../services/caldav';
import { CalendarEvent } from '../types';
import AppIcon, { Icons } from '../components/Icon';

type FilterType = 'all' | 'today' | 'week' | 'month';

export default function EventsScreen() {
  const { state, setEvents, setLoading } = useApp();
  const [filter, setFilter] = useState<FilterType>('all');

  useEffect(() => {
    if (state.caldavConfigured && state.events.length === 0) {
      loadEvents();
    }
  }, [state.caldavConfigured]);

  async function loadEvents() {
    setLoading(true);
    try {
      const events = await caldav.fetchEvents();
      setEvents(events);
    } catch (error) {
      console.error('Error loading events:', error);
    } finally {
      setLoading(false);
    }
  }

  function getFilteredEvents(): CalendarEvent[] {
    const now = new Date();
    const today = new Date(now.getFullYear(), now.getMonth(), now.getDate());
    const weekEnd = new Date(today.getTime() + 7 * 24 * 60 * 60 * 1000);
    const monthEnd = new Date(today.getFullYear(), today.getMonth() + 1, today.getDate());

    let filtered = [...state.events];

    switch (filter) {
      case 'today':
        const tomorrow = new Date(today.getTime() + 24 * 60 * 60 * 1000);
        filtered = filtered.filter((e) => e.start >= today && e.start < tomorrow);
        break;
      case 'week':
        filtered = filtered.filter((e) => e.start >= today && e.start < weekEnd);
        break;
      case 'month':
        filtered = filtered.filter((e) => e.start >= today && e.start < monthEnd);
        break;
    }

    return filtered.sort((a, b) => a.start.getTime() - b.start.getTime());
  }

  function formatEventTime(event: CalendarEvent): string {
    const startDate = event.start.toLocaleDateString();
    const startTime = event.start.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' });
    const endTime = event.end.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' });
    return `${startDate} ${startTime} - ${endTime}`;
  }

  function renderEvent({ item }: { item: CalendarEvent }) {
    return (
      <View style={styles.eventCard}>
        <View style={styles.eventHeader}>
          <Text style={styles.eventTitle}>{item.summary}</Text>
        </View>
        <Text style={styles.eventTime}>{formatEventTime(item)}</Text>
        {item.location && <Text style={styles.eventLocation}>{item.location}</Text>}
        {item.description && (
          <Text style={styles.eventDescription} numberOfLines={2}>
            {item.description}
          </Text>
        )}
      </View>
    );
  }

  if (!state.caldavConfigured) {
    return (
      <View style={styles.centered}>
        <Text style={styles.emptyText}>CalDAV not configured</Text>
        <Text style={styles.hintText}>Go to Settings to add your CalDAV server</Text>
      </View>
    );
  }

  const filteredEvents = getFilteredEvents();

  return (
    <View style={styles.container}>
      <View style={styles.filterBar}>
        {(['all', 'today', 'week', 'month'] as FilterType[]).map((f) => (
          <TouchableOpacity
            key={f}
            style={[styles.filterButton, filter === f && styles.filterActive]}
            onPress={() => setFilter(f)}
          >
            <Text style={[styles.filterText, filter === f && styles.filterTextActive]}>
              {f.charAt(0).toUpperCase() + f.slice(1)}
            </Text>
          </TouchableOpacity>
        ))}
        <TouchableOpacity style={styles.refreshButton} onPress={loadEvents}>
          <AppIcon name={Icons.refresh} size={16} color="#007AFF" />
        </TouchableOpacity>
      </View>

      {state.isLoading ? (
        <ActivityIndicator size="large" color="#007AFF" style={styles.loader} />
      ) : (
        <FlatList
          data={filteredEvents}
          renderItem={renderEvent}
          keyExtractor={(item) => item.uid}
          contentContainerStyle={styles.listContent}
          ListEmptyComponent={
            <View style={styles.centered}>
              <Text style={styles.emptyText}>No events found</Text>
            </View>
          }
        />
      )}
    </View>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: '#f5f5f5',
  },
  centered: {
    flex: 1,
    justifyContent: 'center',
    alignItems: 'center',
    padding: 20,
  },
  filterBar: {
    flexDirection: 'row',
    padding: 12,
    backgroundColor: '#fff',
    borderBottomWidth: 1,
    borderBottomColor: '#e0e0e0',
  },
  filterButton: {
    paddingHorizontal: 12,
    paddingVertical: 6,
    borderRadius: 16,
    marginRight: 8,
    backgroundColor: '#f0f0f0',
  },
  filterActive: {
    backgroundColor: '#007AFF',
  },
  filterText: {
    fontSize: 14,
    color: '#666',
  },
  filterTextActive: {
    color: '#fff',
  },
  refreshButton: {
    marginLeft: 'auto',
    paddingHorizontal: 12,
    paddingVertical: 6,
  },
  refreshText: {
    color: '#007AFF',
    fontWeight: '600',
  },
  listContent: {
    padding: 16,
  },
  eventCard: {
    backgroundColor: '#fff',
    padding: 16,
    borderRadius: 8,
    marginBottom: 12,
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 1 },
    shadowOpacity: 0.1,
    shadowRadius: 2,
    elevation: 2,
  },
  eventHeader: {
    marginBottom: 8,
  },
  eventTitle: {
    fontSize: 16,
    fontWeight: '600',
    color: '#333',
  },
  eventTime: {
    fontSize: 14,
    color: '#007AFF',
    marginBottom: 4,
  },
  eventLocation: {
    fontSize: 14,
    color: '#666',
    marginBottom: 4,
  },
  eventDescription: {
    fontSize: 13,
    color: '#999',
    marginTop: 8,
  },
  emptyText: {
    fontSize: 16,
    color: '#999',
    marginBottom: 8,
  },
  hintText: {
    fontSize: 14,
    color: '#bbb',
  },
  loader: {
    marginTop: 40,
  },
});
