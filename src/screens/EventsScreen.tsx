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
import { useTheme } from '../hooks/useTheme';
import * as caldav from '../services/caldav';
import * as cache from '../services/cache';
import { CalendarEvent, CalDavCalendar } from '../types';
import AppIcon, { Icons } from '../components/Icon';
import EventPropertyPage from '../components/EventPropertyPage';

type FilterType = 'all' | 'today' | 'week' | 'month';

function useCalendarColorMap(calendars: CalDavCalendar[]): Map<string, string> {
  return React.useMemo(() => {
    const map = new Map<string, string>();
    for (const cal of calendars) {
      if (cal.color) map.set(cal.href, cal.color);
    }
    return map;
  }, [calendars]);
}

export default function EventsScreen() {
  const { state, setEvents, setLoading } = useApp();
  const theme = useTheme();
  const [filter, setFilter] = useState<FilterType>('all');
  const [selectedEvent, setSelectedEvent] = useState<CalendarEvent | null>(null);
  const calColorMap = useCalendarColorMap(state.selectedCalendars);

  useEffect(() => {
    if (state.caldavConfigured && state.events.length === 0) {
      loadEvents();
    }
  }, [state.caldavConfigured]);

  async function loadEvents() {
    setLoading(true);
    try {
      const eventHrefs = state.selectedCalendars
        .filter((c: CalDavCalendar) => c.components.includes('VEVENT'))
        .map((c: CalDavCalendar) => c.href);
      const events = await caldav.fetchEvents(eventHrefs.length > 0 ? eventHrefs : undefined);
      setEvents(events);
      await cache.saveEvents(events);
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
      case 'today': {
        const tomorrow = new Date(today.getTime() + 24 * 60 * 60 * 1000);
        filtered = filtered.filter((e) => e.start >= today && e.start < tomorrow);
        break;
      }
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

  const c = theme.colors;

  function renderEvent({ item }: { item: CalendarEvent }) {
    const calColor = item.calendarHref ? calColorMap.get(item.calendarHref) : undefined;
    return (
      <TouchableOpacity style={[styles.eventCard, { backgroundColor: c.surface }]} onPress={() => setSelectedEvent(item)}>
        <View style={styles.eventHeader}>
          {calColor && <View style={[styles.eventCalDot, { backgroundColor: calColor }]} />}
          <Text style={[styles.eventTitle, { color: c.text }]}>{item.summary}</Text>
        </View>
        <Text style={[styles.eventTime, { color: c.primary }]}>{formatEventTime(item)}</Text>
        {item.location && <Text style={[styles.eventLocation, { color: c.textSecondary }]}>{item.location}</Text>}
        {item.description && (
          <Text style={[styles.eventDescription, { color: c.textTertiary }]} numberOfLines={2}>
            {item.description}
          </Text>
        )}
      </TouchableOpacity>
    );
  }

  if (!state.caldavConfigured) {
    return (
      <View style={[styles.centered, { backgroundColor: c.background }]}>
        <Text style={[styles.emptyText, { color: c.textTertiary }]}>CalDAV not configured</Text>
        <Text style={[styles.hintText, { color: c.textQuaternary }]}>Go to Settings to add your CalDAV server</Text>
      </View>
    );
  }

  const filteredEvents = getFilteredEvents();

  return (
    <View style={[styles.container, { backgroundColor: c.background }]}>
      <View style={[styles.filterBar, { backgroundColor: c.surface, borderBottomColor: c.border }]}>
        {(['all', 'today', 'week', 'month'] as FilterType[]).map((f) => (
          <TouchableOpacity
            key={f}
            style={[styles.filterButton, { backgroundColor: c.filterChip }, filter === f && { backgroundColor: c.primary }]}
            onPress={() => setFilter(f)}
          >
            <Text style={[styles.filterText, { color: filter === f ? '#fff' : c.textSecondary }]}>
              {f.charAt(0).toUpperCase() + f.slice(1)}
            </Text>
          </TouchableOpacity>
        ))}
        <TouchableOpacity style={styles.refreshButton} onPress={loadEvents}>
          <AppIcon name={Icons.refresh} size={16} color={c.primary} />
        </TouchableOpacity>
      </View>

      {state.isLoading ? (
        <ActivityIndicator size="large" color={c.primary} style={styles.loader} />
      ) : (
        <FlatList
          data={filteredEvents}
          renderItem={renderEvent}
          keyExtractor={(item) => item.uid}
          contentContainerStyle={styles.listContent}
          ListEmptyComponent={
            <View style={styles.centered}>
              <Text style={[styles.emptyText, { color: c.textTertiary }]}>No events found</Text>
            </View>
          }
        />
      )}

      {selectedEvent && (
        <EventPropertyPage
          event={selectedEvent}
          calendars={state.selectedCalendars}
          onClose={() => setSelectedEvent(null)}
        />
      )}
    </View>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
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
    borderBottomWidth: 1,
  },
  filterButton: {
    paddingHorizontal: 12,
    paddingVertical: 6,
    borderRadius: 16,
    marginRight: 8,
  },
  filterText: {
    fontSize: 14,
  },
  refreshButton: {
    marginLeft: 'auto',
    paddingHorizontal: 12,
    paddingVertical: 6,
  },
  listContent: {
    padding: 16,
  },
  eventCard: {
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
    flexDirection: 'row',
    alignItems: 'center',
    gap: 8,
    marginBottom: 8,
  },
  eventCalDot: {
    width: 10,
    height: 10,
    borderRadius: 5,
  },
  eventTitle: {
    fontSize: 16,
    fontWeight: '600',
    flex: 1,
  },
  eventTime: {
    fontSize: 14,
    marginBottom: 4,
  },
  eventLocation: {
    fontSize: 14,
    marginBottom: 4,
  },
  eventDescription: {
    fontSize: 13,
    marginTop: 8,
  },
  emptyText: {
    fontSize: 16,
    marginBottom: 8,
  },
  hintText: {
    fontSize: 14,
  },
  loader: {
    marginTop: 40,
  },
});
