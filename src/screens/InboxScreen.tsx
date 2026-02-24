import React, { useEffect, useState, useMemo } from 'react';
import {
  View,
  Text,
  StyleSheet,
  FlatList,
  TouchableOpacity,
  ActivityIndicator,
  Linking,
  TextInput,
  Platform,
} from 'react-native';
import { useApp } from '../store/AppContext';
import * as caldav from '../services/caldav';
import * as gitea from '../services/gitea';
import { InboxItem, InboxItemType, GiteaMilestone } from '../types';
import AppIcon, { Icons } from '../components/Icon';

const TYPE_COLORS: Record<InboxItemType, string> = {
  event: '#4CAF50',
  issue: '#2196F3',
  milestone: '#9C27B0',
  task: '#FF9800',
};

const TYPE_ICONS: Record<InboxItemType, string> = {
  event: Icons.calendar,
  issue: Icons.issues,
  milestone: 'mdi:flag',
  task: Icons.check,
};

export default function InboxScreen() {
  const { state, setEvents, setIssues, setLoading } = useApp();
  const [milestones, setMilestones] = useState<GiteaMilestone[]>([]);
  const [selectedTypes, setSelectedTypes] = useState<InboxItemType[]>(['event', 'issue', 'milestone', 'task']);
  const [dateFrom, setDateFrom] = useState('');
  const [dateTo, setDateTo] = useState('');
  const [showFilters, setShowFilters] = useState(false);

  useEffect(() => {
    loadAllData();
  }, []);

  async function loadAllData() {
    setLoading(true);
    try {
      const promises: Promise<void>[] = [];

      if (state.caldavConfigured) {
        promises.push(
          caldav.fetchEvents().then((events) => setEvents(events))
        );
      }

      if (state.giteaConfigured && state.giteaRepositories.length > 0) {
        promises.push(
          gitea.fetchAllIssues(state.giteaRepositories).then((issues) => setIssues(issues))
        );
        promises.push(
          gitea.fetchAllMilestones(state.giteaRepositories).then((m) => setMilestones(m))
        );
      }

      await Promise.all(promises);
    } catch (error) {
      console.error('Error loading inbox data:', error);
    } finally {
      setLoading(false);
    }
  }

  const inboxItems = useMemo((): InboxItem[] => {
    const items: InboxItem[] = [];

    // Add calendar events
    state.events.forEach((event) => {
      items.push({
        id: `event-${event.uid}`,
        type: 'event',
        title: event.summary,
        description: event.description,
        date: event.start,
        endDate: event.end,
        source: 'CalDAV',
      });
    });

    // Add issues
    state.issues.forEach((issue) => {
      items.push({
        id: `issue-${issue.id}`,
        type: 'issue',
        title: `#${issue.number} ${issue.title}`,
        description: issue.body,
        date: issue.createdAt,
        state: issue.state,
        source: issue.repository,
        sourceUrl: issue.htmlUrl,
        labels: issue.labels,
      });
    });

    // Add milestones
    milestones.forEach((milestone) => {
      items.push({
        id: `milestone-${milestone.id}`,
        type: 'milestone',
        title: milestone.title,
        description: `${milestone.openIssues} open, ${milestone.closedIssues} closed`,
        date: milestone.dueOn || new Date(),
        state: milestone.state,
        source: milestone.repository,
        sourceUrl: milestone.htmlUrl,
      });
    });

    // Add tasks from notes (simplified - tasks could be extracted from notes with checkboxes)
    state.notes.forEach((note) => {
      if (note.title.toLowerCase().includes('task') || note.content.includes('[ ]')) {
        items.push({
          id: `task-${note.uid}`,
          type: 'task',
          title: note.title,
          description: note.content,
          date: note.createdAt,
          source: 'Notes',
        });
      }
    });

    return items;
  }, [state.events, state.issues, state.notes, milestones]);

  const filteredItems = useMemo(() => {
    let items = inboxItems.filter((item) => selectedTypes.includes(item.type));

    if (dateFrom) {
      const from = new Date(dateFrom);
      items = items.filter((item) => item.date >= from);
    }

    if (dateTo) {
      const to = new Date(dateTo);
      to.setHours(23, 59, 59);
      items = items.filter((item) => item.date <= to);
    }

    return items.sort((a, b) => a.date.getTime() - b.date.getTime());
  }, [inboxItems, selectedTypes, dateFrom, dateTo]);

  function toggleType(type: InboxItemType) {
    setSelectedTypes((prev) =>
      prev.includes(type) ? prev.filter((t) => t !== type) : [...prev, type]
    );
  }

  function openItem(item: InboxItem) {
    if (item.sourceUrl) {
      Linking.openURL(item.sourceUrl);
    }
  }

  function formatDate(date: Date): string {
    const today = new Date();
    const tomorrow = new Date(today);
    tomorrow.setDate(tomorrow.getDate() + 1);

    if (date.toDateString() === today.toDateString()) {
      return `Today ${date.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' })}`;
    }
    if (date.toDateString() === tomorrow.toDateString()) {
      return `Tomorrow ${date.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' })}`;
    }
    return date.toLocaleDateString([], { month: 'short', day: 'numeric', year: 'numeric' });
  }

  function renderItem({ item }: { item: InboxItem }) {
    return (
      <TouchableOpacity
        style={styles.itemCard}
        onPress={() => openItem(item)}
        disabled={!item.sourceUrl}
      >
        <View style={styles.itemHeader}>
          <View style={[styles.typeBadge, { backgroundColor: TYPE_COLORS[item.type] }]}>
            <AppIcon name={TYPE_ICONS[item.type]} size={14} color="#fff" />
            <Text style={styles.typeText}>{item.type}</Text>
          </View>
          {item.state && (
            <View style={[styles.stateBadge, item.state === 'open' ? styles.stateOpen : styles.stateClosed]}>
              <Text style={styles.stateText}>{item.state}</Text>
            </View>
          )}
        </View>

        <Text style={styles.itemTitle}>{item.title}</Text>

        {item.description && (
          <Text style={styles.itemDescription} numberOfLines={2}>
            {item.description}
          </Text>
        )}

        <View style={styles.itemFooter}>
          <View style={styles.dateContainer}>
            <AppIcon name={Icons.time} size={14} color="#999" />
            <Text style={styles.itemDate}>{formatDate(item.date)}</Text>
          </View>
          <Text style={styles.itemSource}>{item.source}</Text>
        </View>

        {item.labels && item.labels.length > 0 && (
          <View style={styles.labelContainer}>
            {item.labels.slice(0, 3).map((label) => (
              <View key={label.id} style={[styles.label, { backgroundColor: `#${label.color}` }]}>
                <Text style={styles.labelText}>{label.name}</Text>
              </View>
            ))}
          </View>
        )}
      </TouchableOpacity>
    );
  }

  return (
    <View style={styles.container}>
      <View style={styles.header}>
        <Text style={styles.title}>Inbox</Text>
        <View style={styles.headerActions}>
          <TouchableOpacity style={styles.filterToggle} onPress={() => setShowFilters(!showFilters)}>
            <AppIcon name={Icons.filter} size={20} color={showFilters ? '#007AFF' : '#666'} />
          </TouchableOpacity>
          <TouchableOpacity style={styles.refreshButton} onPress={loadAllData}>
            <AppIcon name={Icons.refresh} size={20} color="#007AFF" />
          </TouchableOpacity>
        </View>
      </View>

      {showFilters && (
        <View style={styles.filterContainer}>
          <Text style={styles.filterLabel}>Type</Text>
          <View style={styles.typeFilters}>
            {(['event', 'issue', 'milestone', 'task'] as InboxItemType[]).map((type) => (
              <TouchableOpacity
                key={type}
                style={[
                  styles.typeFilterButton,
                  selectedTypes.includes(type) && { backgroundColor: TYPE_COLORS[type] },
                ]}
                onPress={() => toggleType(type)}
              >
                <AppIcon
                  name={TYPE_ICONS[type]}
                  size={16}
                  color={selectedTypes.includes(type) ? '#fff' : '#666'}
                />
                <Text
                  style={[
                    styles.typeFilterText,
                    selectedTypes.includes(type) && styles.typeFilterTextActive,
                  ]}
                >
                  {type}
                </Text>
              </TouchableOpacity>
            ))}
          </View>

          <Text style={styles.filterLabel}>Date Range</Text>
          <View style={styles.dateFilters}>
            <TextInput
              style={styles.dateInput}
              value={dateFrom}
              onChangeText={setDateFrom}
              placeholder="From (YYYY-MM-DD)"
              placeholderTextColor="#999"
            />
            <Text style={styles.dateSeparator}>to</Text>
            <TextInput
              style={styles.dateInput}
              value={dateTo}
              onChangeText={setDateTo}
              placeholder="To (YYYY-MM-DD)"
              placeholderTextColor="#999"
            />
          </View>

          {(dateFrom || dateTo) && (
            <TouchableOpacity
              style={styles.clearDates}
              onPress={() => {
                setDateFrom('');
                setDateTo('');
              }}
            >
              <Text style={styles.clearDatesText}>Clear dates</Text>
            </TouchableOpacity>
          )}
        </View>
      )}

      <View style={styles.summary}>
        <Text style={styles.summaryText}>
          {filteredItems.length} item{filteredItems.length !== 1 ? 's' : ''}
        </Text>
      </View>

      {state.isLoading ? (
        <ActivityIndicator size="large" color="#007AFF" style={styles.loader} />
      ) : (
        <FlatList
          data={filteredItems}
          renderItem={renderItem}
          keyExtractor={(item) => item.id}
          contentContainerStyle={styles.listContent}
          ListEmptyComponent={
            <View style={styles.emptyContainer}>
              <AppIcon name="mdi:inbox" size={48} color="#ccc" />
              <Text style={styles.emptyText}>No items in inbox</Text>
              <Text style={styles.emptyHint}>Configure CalDAV or Gitea in Settings</Text>
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
  headerActions: {
    flexDirection: 'row',
    gap: 12,
  },
  filterToggle: {
    padding: 8,
  },
  refreshButton: {
    padding: 8,
  },
  filterContainer: {
    backgroundColor: '#fff',
    padding: 16,
    borderBottomWidth: 1,
    borderBottomColor: '#e0e0e0',
  },
  filterLabel: {
    fontSize: 14,
    fontWeight: '600',
    color: '#666',
    marginBottom: 8,
  },
  typeFilters: {
    flexDirection: 'row',
    flexWrap: 'wrap',
    gap: 8,
    marginBottom: 16,
  },
  typeFilterButton: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 6,
    paddingHorizontal: 12,
    paddingVertical: 6,
    borderRadius: 16,
    backgroundColor: '#f0f0f0',
  },
  typeFilterText: {
    fontSize: 13,
    color: '#666',
    textTransform: 'capitalize',
  },
  typeFilterTextActive: {
    color: '#fff',
  },
  dateFilters: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 8,
  },
  dateInput: {
    flex: 1,
    borderWidth: 1,
    borderColor: '#ddd',
    borderRadius: 8,
    padding: Platform.OS === 'web' ? 10 : 8,
    fontSize: 14,
    backgroundColor: '#fafafa',
  },
  dateSeparator: {
    color: '#999',
  },
  clearDates: {
    marginTop: 8,
  },
  clearDatesText: {
    color: '#007AFF',
    fontSize: 14,
  },
  summary: {
    padding: 12,
    paddingHorizontal: 16,
    backgroundColor: '#fafafa',
    borderBottomWidth: 1,
    borderBottomColor: '#e0e0e0',
  },
  summaryText: {
    fontSize: 13,
    color: '#666',
  },
  listContent: {
    padding: 16,
  },
  itemCard: {
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
  itemHeader: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 8,
    marginBottom: 8,
  },
  typeBadge: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 4,
    paddingHorizontal: 8,
    paddingVertical: 3,
    borderRadius: 12,
  },
  typeText: {
    fontSize: 11,
    color: '#fff',
    fontWeight: '600',
    textTransform: 'capitalize',
  },
  stateBadge: {
    paddingHorizontal: 8,
    paddingVertical: 2,
    borderRadius: 12,
  },
  stateOpen: {
    backgroundColor: '#E8F5E9',
  },
  stateClosed: {
    backgroundColor: '#ECEFF1',
  },
  stateText: {
    fontSize: 11,
    color: '#666',
    textTransform: 'capitalize',
  },
  itemTitle: {
    fontSize: 16,
    fontWeight: '600',
    color: '#333',
    marginBottom: 4,
  },
  itemDescription: {
    fontSize: 14,
    color: '#666',
    marginBottom: 8,
    lineHeight: 20,
  },
  itemFooter: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
  },
  dateContainer: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 4,
  },
  itemDate: {
    fontSize: 13,
    color: '#999',
  },
  itemSource: {
    fontSize: 12,
    color: '#999',
  },
  labelContainer: {
    flexDirection: 'row',
    flexWrap: 'wrap',
    marginTop: 8,
    gap: 4,
  },
  label: {
    paddingHorizontal: 8,
    paddingVertical: 2,
    borderRadius: 12,
  },
  labelText: {
    fontSize: 10,
    color: '#fff',
    fontWeight: '600',
  },
  loader: {
    marginTop: 40,
  },
  emptyContainer: {
    alignItems: 'center',
    paddingTop: 60,
  },
  emptyText: {
    fontSize: 16,
    color: '#999',
    marginTop: 16,
  },
  emptyHint: {
    fontSize: 14,
    color: '#bbb',
    marginTop: 4,
  },
});
