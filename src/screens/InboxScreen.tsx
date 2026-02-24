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
import { useTheme } from '../hooks/useTheme';
import * as gitea from '../services/gitea';
import { useSyncAll } from '../hooks/useSyncAll';
import { InboxItem, InboxItemType, GiteaMilestone } from '../types';
import AppIcon, { Icons } from '../components/Icon';

// Parse time-estimate tags like #20m, #2h — returns minutes or null
function parseTimeTag(tag: string): number | null {
  const match = tag.match(/^(\d+)(m|h)$/i);
  if (!match) return null;
  const value = parseInt(match[1], 10);
  return match[2].toLowerCase() === 'h' ? value * 60 : value;
}

function isAllDayEvent(start: Date, end: Date): boolean {
  const durationMs = end.getTime() - start.getTime();
  if (durationMs < 24 * 60 * 60 * 1000) return false;
  return (
    start.getHours() === 0 && start.getMinutes() === 0 && start.getSeconds() === 0 &&
    end.getHours() === 0 && end.getMinutes() === 0 && end.getSeconds() === 0
  );
}

function formatTotalTime(minutes: number): string {
  if (minutes === 0) return '0m';
  const h = Math.floor(minutes / 60);
  const m = Math.round(minutes % 60);
  if (h === 0) return `${m}m`;
  if (m === 0) return `${h}h`;
  return `${h}h ${m}m`;
}

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
  const { state } = useApp();
  const theme = useTheme();
  const { syncAll } = useSyncAll();
  const [milestones, setMilestones] = useState<GiteaMilestone[]>([]);
  const [selectedTypes, setSelectedTypes] = useState<InboxItemType[]>(['event', 'issue', 'milestone', 'task']);
  const [dateFrom, setDateFrom] = useState('');
  const [dateTo, setDateTo] = useState('');
  const [showFilters, setShowFilters] = useState(false);
  const [activeOnly, setActiveOnly] = useState(false);

  useEffect(() => {
    loadAllData();
  }, []);

  async function loadAllData() {
    const promises: Promise<void>[] = [syncAll()];
    if (state.giteaConfigured && state.giteaRepositories.length > 0) {
      promises.push(
        gitea.fetchAllMilestones(state.giteaRepositories)
          .then((m) => setMilestones(m))
          .catch(() => {})
      );
    }
    await Promise.all(promises);
  }

  const inboxItems = useMemo((): InboxItem[] => {
    const items: InboxItem[] = [];

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

    state.tasks.forEach((task) => {
      items.push({
        id: `task-${task.uid}`,
        type: 'task',
        title: task.summary,
        description: task.description,
        date: task.due || task.created,
        state: (task.status === 'COMPLETED' || task.status === 'CANCELLED') ? 'closed' : 'open',
        source: 'CalDAV Tasks',
        priority: task.priority >= 1 && task.priority <= 4 ? 'high' : task.priority === 5 ? 'medium' : 'low',
      });
    });

    return items;
  }, [state.events, state.issues, state.tasks, milestones]);

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

    if (activeOnly) {
      const now = new Date();
      items = items.filter((item) => {
        if (item.type === 'event') return item.date >= now;
        return item.state === 'open';
      });
    }

    return items.sort((a, b) => a.date.getTime() - b.date.getTime());
  }, [inboxItems, selectedTypes, dateFrom, dateTo, activeOnly]);

  // Total time estimate from filtered items
  const totalMinutes = useMemo(() => {
    let total = 0;

    // Build task categories lookup by uid
    const taskCatMap = new Map<string, string[]>();
    for (const t of state.tasks) {
      taskCatMap.set(t.uid, t.categories || []);
    }

    for (const item of filteredItems) {
      if (item.type === 'event') {
        if (item.date && item.endDate) {
          if (isAllDayEvent(item.date, item.endDate)) continue;
          const mins = (item.endDate.getTime() - item.date.getTime()) / 60000;
          if (mins > 0) total += mins;
        }
      } else if (item.type === 'task') {
        const uid = item.id.replace('task-', '');
        const categories = taskCatMap.get(uid) || [];
        for (const cat of categories) {
          const mins = parseTimeTag(cat);
          if (mins !== null) { total += mins; break; }
        }
      } else if (item.type === 'issue') {
        if (item.labels) {
          for (const label of item.labels) {
            const mins = parseTimeTag(label.name);
            if (mins !== null) { total += mins; break; }
          }
        }
      }
    }

    return total;
  }, [filteredItems, state.tasks]);

  function toDateString(d: Date): string {
    const y = d.getFullYear();
    const m = String(d.getMonth() + 1).padStart(2, '0');
    const day = String(d.getDate()).padStart(2, '0');
    return `${y}-${m}-${day}`;
  }

  function applyDatePreset(preset: string) {
    const today = new Date();
    today.setHours(0, 0, 0, 0);
    switch (preset) {
      case 'today':
        setDateFrom(toDateString(today));
        setDateTo(toDateString(today));
        break;
      case 'tomorrow': {
        const tom = new Date(today);
        tom.setDate(today.getDate() + 1);
        setDateFrom(toDateString(tom));
        setDateTo(toDateString(tom));
        break;
      }
      case 'overdue': {
        const yesterday = new Date(today);
        yesterday.setDate(today.getDate() - 1);
        setDateFrom('');
        setDateTo(toDateString(yesterday));
        break;
      }
      case 'yesterday': {
        const y = new Date(today);
        y.setDate(y.getDate() - 1);
        setDateFrom(toDateString(y));
        setDateTo(toDateString(y));
        break;
      }
      case 'thisWeek': {
        const start = new Date(today);
        start.setDate(today.getDate() - today.getDay());
        const end = new Date(start);
        end.setDate(start.getDate() + 6);
        setDateFrom(toDateString(start));
        setDateTo(toDateString(end));
        break;
      }
      case 'thisMonth': {
        const start = new Date(today.getFullYear(), today.getMonth(), 1);
        const end = new Date(today.getFullYear(), today.getMonth() + 1, 0);
        setDateFrom(toDateString(start));
        setDateTo(toDateString(end));
        break;
      }
      case 'next7': {
        const end = new Date(today);
        end.setDate(today.getDate() + 6);
        setDateFrom(toDateString(today));
        setDateTo(toDateString(end));
        break;
      }
    }
  }

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

  const c = theme.colors;

  function renderItem({ item }: { item: InboxItem }) {
    return (
      <TouchableOpacity
        style={[styles.itemCard, { backgroundColor: c.surface }]}
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
              <Text style={[styles.stateText, { color: c.textSecondary }]}>{item.state}</Text>
            </View>
          )}
        </View>

        <Text style={[styles.itemTitle, { color: c.text }]}>{item.title}</Text>

        {item.description && (
          <Text style={[styles.itemDescription, { color: c.textSecondary }]} numberOfLines={2}>
            {item.description}
          </Text>
        )}

        <View style={styles.itemFooter}>
          <View style={styles.dateContainer}>
            <AppIcon name={Icons.time} size={14} color={c.textTertiary} />
            <Text style={[styles.itemDate, { color: c.textTertiary }]}>{formatDate(item.date)}</Text>
          </View>
          <Text style={[styles.itemSource, { color: c.textTertiary }]}>{item.source}</Text>
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
    <View style={[styles.container, { backgroundColor: c.background }]}>
      <View style={[styles.header, { backgroundColor: c.surface, borderBottomColor: c.border }]}>
        <Text style={[styles.title, { color: c.text }]}>Inbox</Text>
        <View style={styles.headerActions}>
          <TouchableOpacity style={styles.filterToggle} onPress={() => setShowFilters(!showFilters)}>
            <AppIcon name={Icons.filter} size={20} color={showFilters ? c.primary : c.textSecondary} />
          </TouchableOpacity>
          <TouchableOpacity style={styles.refreshButton} onPress={loadAllData}>
            <AppIcon name={Icons.refresh} size={20} color={c.primary} />
          </TouchableOpacity>
        </View>
      </View>

      {showFilters && (
        <View style={[styles.filterContainer, { backgroundColor: c.surface, borderBottomColor: c.border }]}>
          <Text style={[styles.filterLabel, { color: c.textSecondary }]}>Type</Text>
          <View style={styles.typeFilters}>
            {(['event', 'issue', 'milestone', 'task'] as InboxItemType[]).map((type) => (
              <TouchableOpacity
                key={type}
                style={[
                  styles.typeFilterButton,
                  { backgroundColor: c.filterChip },
                  selectedTypes.includes(type) && { backgroundColor: TYPE_COLORS[type] },
                ]}
                onPress={() => toggleType(type)}
              >
                <AppIcon
                  name={TYPE_ICONS[type]}
                  size={16}
                  color={selectedTypes.includes(type) ? '#fff' : c.textSecondary}
                />
                <Text
                  style={[
                    styles.typeFilterText,
                    { color: selectedTypes.includes(type) ? '#fff' : c.textSecondary },
                  ]}
                >
                  {type}
                </Text>
              </TouchableOpacity>
            ))}
          </View>

          <TouchableOpacity
            style={[
              styles.activeOnlyToggle,
              { backgroundColor: activeOnly ? c.primary : c.filterChip },
            ]}
            onPress={() => setActiveOnly((v) => !v)}
          >
            <AppIcon name={Icons.check} size={14} color={activeOnly ? '#fff' : c.textSecondary} />
            <Text style={[styles.activeOnlyText, { color: activeOnly ? '#fff' : c.textSecondary }]}>
              Active only
            </Text>
          </TouchableOpacity>

          <Text style={[styles.filterLabel, { color: c.textSecondary, marginTop: 16 }]}>Date Range</Text>
          <View style={styles.datePresets}>
            {[
              { key: 'today', label: 'Today' },
              { key: 'tomorrow', label: 'Tomorrow' },
              { key: 'overdue', label: 'Overdue' },
              { key: 'yesterday', label: 'Yesterday' },
              { key: 'thisWeek', label: 'This Week' },
              { key: 'thisMonth', label: 'This Month' },
              { key: 'next7', label: 'Next 7 Days' },
            ].map((preset) => (
              <TouchableOpacity
                key={preset.key}
                style={[styles.presetChip, { backgroundColor: c.filterChip }]}
                onPress={() => applyDatePreset(preset.key)}
              >
                <Text style={[styles.presetChipText, { color: c.textSecondary }]}>{preset.label}</Text>
              </TouchableOpacity>
            ))}
          </View>
          <View style={styles.dateFilters}>
            <TextInput
              style={[styles.dateInput, { borderColor: c.border, backgroundColor: c.inputBackground, color: c.text }]}
              value={dateFrom}
              onChangeText={setDateFrom}
              placeholder="From (YYYY-MM-DD)"
              placeholderTextColor={c.textTertiary}
            />
            <Text style={[styles.dateSeparator, { color: c.textTertiary }]}>to</Text>
            <TextInput
              style={[styles.dateInput, { borderColor: c.border, backgroundColor: c.inputBackground, color: c.text }]}
              value={dateTo}
              onChangeText={setDateTo}
              placeholder="To (YYYY-MM-DD)"
              placeholderTextColor={c.textTertiary}
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
              <Text style={[styles.clearDatesText, { color: c.primary }]}>Clear dates</Text>
            </TouchableOpacity>
          )}
        </View>
      )}

      <View style={[styles.summary, { backgroundColor: c.background, borderBottomColor: c.border }]}>
        <Text style={[styles.summaryText, { color: c.textSecondary }]}>
          {filteredItems.length} item{filteredItems.length !== 1 ? 's' : ''}
        </Text>
      </View>

      {state.isLoading ? (
        <ActivityIndicator size="large" color={c.primary} style={styles.loader} />
      ) : (
        <FlatList
          data={filteredItems}
          renderItem={renderItem}
          keyExtractor={(item) => item.id}
          contentContainerStyle={styles.listContent}
          ListFooterComponent={
            filteredItems.length > 0 ? (
              <View style={[styles.totalTimeContainer, { backgroundColor: c.surface, borderColor: c.border }]}>
                <AppIcon name={Icons.timer} size={18} color={c.primary} />
                <Text style={[styles.totalTimeLabel, { color: c.textSecondary }]}>Total estimated time:</Text>
                <Text style={[styles.totalTimeValue, { color: c.text }]}>{formatTotalTime(totalMinutes)}</Text>
              </View>
            ) : null
          }
          ListEmptyComponent={
            <View style={styles.emptyContainer}>
              <AppIcon name="mdi:inbox" size={48} color={c.textQuaternary} />
              <Text style={[styles.emptyText, { color: c.textTertiary }]}>No items in inbox</Text>
              <Text style={[styles.emptyHint, { color: c.textQuaternary }]}>Configure CalDAV or Gitea in Settings</Text>
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
    padding: 16,
    borderBottomWidth: 1,
  },
  filterLabel: {
    fontSize: 14,
    fontWeight: '600',
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
  },
  typeFilterText: {
    fontSize: 13,
    textTransform: 'capitalize',
  },
  dateFilters: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 8,
  },
  dateInput: {
    flex: 1,
    borderWidth: 1,
    borderRadius: 8,
    padding: Platform.OS === 'web' ? 10 : 8,
    fontSize: 14,
  },
  dateSeparator: {
    fontSize: 14,
  },
  activeOnlyToggle: {
    flexDirection: 'row',
    alignItems: 'center',
    alignSelf: 'flex-start',
    gap: 6,
    paddingHorizontal: 14,
    paddingVertical: 7,
    borderRadius: 16,
    marginBottom: 4,
  },
  activeOnlyText: {
    fontSize: 13,
    fontWeight: '600',
  },
  datePresets: {
    flexDirection: 'row',
    flexWrap: 'wrap',
    gap: 8,
    marginBottom: 10,
  },
  presetChip: {
    paddingHorizontal: 12,
    paddingVertical: 6,
    borderRadius: 14,
  },
  presetChipText: {
    fontSize: 12,
    fontWeight: '500',
  },
  clearDates: {
    marginTop: 8,
  },
  clearDatesText: {
    fontSize: 14,
  },
  summary: {
    padding: 12,
    paddingHorizontal: 16,
    borderBottomWidth: 1,
  },
  summaryText: {
    fontSize: 13,
  },
  listContent: {
    padding: 16,
  },
  itemCard: {
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
    textTransform: 'capitalize',
  },
  itemTitle: {
    fontSize: 16,
    fontWeight: '600',
    marginBottom: 4,
  },
  itemDescription: {
    fontSize: 14,
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
  },
  itemSource: {
    fontSize: 12,
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
  totalTimeContainer: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 8,
    padding: 14,
    borderRadius: 8,
    borderWidth: 1,
    marginTop: 4,
    marginBottom: 16,
  },
  totalTimeLabel: {
    fontSize: 14,
  },
  totalTimeValue: {
    fontSize: 16,
    fontWeight: '700',
  },
  emptyContainer: {
    alignItems: 'center',
    paddingTop: 60,
  },
  emptyText: {
    fontSize: 16,
    marginTop: 16,
  },
  emptyHint: {
    fontSize: 14,
    marginTop: 4,
  },
});
