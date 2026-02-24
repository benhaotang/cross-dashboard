import React, { useState, useEffect, useMemo, useCallback } from 'react';
import {
  View,
  Text,
  StyleSheet,
  ScrollView,
  TouchableOpacity,
  Modal,
  TextInput,
  ActivityIndicator,
  Dimensions,
} from 'react-native';

const WINDOW_HEIGHT = Dimensions.get('window').height;
import { useApp } from '../store/AppContext';
import { useTheme } from '../hooks/useTheme';
import { CalDavTask, GiteaIssue } from '../types';
import AppIcon, { Icons } from '../components/Icon';
import TaskPropertyPage from '../components/TaskPropertyPage';
import IssuePropertyPage from '../components/IssuePropertyPage';
import * as cache from '../services/cache';
import * as caldav from '../services/caldav';
import * as gitea from '../services/gitea';

type ViewMode = 'kanban' | 'quadrants';

const DEFAULT_COLUMNS = ['backlog', 'planned', 'inprogress', 'done'];
const COLUMN_COLORS = ['#6366F1', '#F59E0B', '#3B82F6', '#10B981'];

interface QuadrantDef {
  label: string;
  subtitle: string;
  tag: string;
  color: string;
}

const QUADRANTS: QuadrantDef[] = [
  { label: 'Do', subtitle: 'Urgent & Important', tag: 'do', color: '#EF4444' },
  { label: 'Delay', subtitle: 'Not Urgent & Important', tag: 'delay', color: '#3B82F6' },
  { label: 'Delegate', subtitle: 'Urgent & Not Important', tag: 'delegate', color: '#F97316' },
  { label: 'Eliminate', subtitle: 'Not Urgent & Not Important', tag: 'eliminate', color: '#6B7280' },
];

const QUADRANT_TAGS = QUADRANTS.map((q) => q.tag.toLowerCase());

// Unified item wrapping either a task or issue
type ViewItem =
  | { kind: 'task'; task: CalDavTask }
  | { kind: 'issue'; issue: GiteaIssue };

function getItemId(item: ViewItem): string {
  return item.kind === 'task' ? `task:${item.task.uid}` : `issue:${item.issue.id}`;
}

function getItemTitle(item: ViewItem): string {
  return item.kind === 'task' ? item.task.summary : item.issue.title;
}

function getItemTags(item: ViewItem): string[] {
  if (item.kind === 'task') return item.task.categories || [];
  return item.issue.labels.map((l) => l.name);
}

function itemHasTag(item: ViewItem, tag: string): boolean {
  return getItemTags(item).some((t) => t.toLowerCase() === tag.toLowerCase());
}

function getPriorityColor(priority: number): string | null {
  if (priority >= 1 && priority <= 4) return '#F44336';
  if (priority === 5) return '#FF9800';
  if (priority >= 6 && priority <= 9) return '#2196F3';
  return null;
}

// Card component for both tasks and issues
function ItemCard({
  item,
  onPress,
}: {
  item: ViewItem;
  onPress: () => void;
}) {
  const theme = useTheme();
  const c = theme.colors;
  const isTask = item.kind === 'task';
  const isCompleted = isTask && item.task.status === 'COMPLETED';
  const priorityColor = isTask ? getPriorityColor(item.task.priority) : null;
  const dueDate = isTask ? item.task.due : undefined;

  return (
    <TouchableOpacity
      style={[styles.taskCard, { backgroundColor: c.surface }]}
      onPress={onPress}
    >
      <View style={styles.taskCardRow}>
        <AppIcon
          name={isTask ? Icons.task : Icons.issues}
          size={14}
          color={c.textTertiary}
        />
        {priorityColor && (
          <View style={[styles.priorityDot, { backgroundColor: priorityColor }]} />
        )}
        <Text
          style={[
            styles.taskCardTitle,
            { color: c.text },
            isCompleted && styles.taskCardTitleCompleted,
          ]}
          numberOfLines={2}
        >
          {getItemTitle(item)}
        </Text>
      </View>
      {dueDate && (
        <Text style={[styles.taskCardDue, { color: c.textTertiary }]}>
          {dueDate.toLocaleDateString([], { month: 'short', day: 'numeric' })}
        </Text>
      )}
      {!isTask && (
        <Text style={[styles.taskCardDue, { color: c.textTertiary }]} numberOfLines={1}>
          {item.issue.repository} #{item.issue.number}
        </Text>
      )}
    </TouchableOpacity>
  );
}

function KanbanColumn({
  title,
  color,
  items,
  onItemPress,
  onAddPress,
}: {
  title: string;
  color: string;
  items: ViewItem[];
  onItemPress: (item: ViewItem) => void;
  onAddPress: () => void;
}) {
  const theme = useTheme();
  const c = theme.colors;

  return (
    <View style={[styles.kanbanColumn, { backgroundColor: c.background, borderColor: c.border }]}>
      <View style={[styles.kanbanColumnHeader, { backgroundColor: color }]}>
        <Text style={styles.kanbanColumnTitle}>#{title}</Text>
        <View style={styles.kanbanColumnHeaderRight}>
          <Text style={styles.kanbanColumnCount}>{items.length}</Text>
          <TouchableOpacity onPress={onAddPress} style={styles.columnAddButton}>
            <AppIcon name={Icons.add} size={16} color="#fff" />
          </TouchableOpacity>
        </View>
      </View>
      <ScrollView style={styles.kanbanColumnBody} contentContainerStyle={styles.kanbanColumnContent}>
        {items.map((item) => (
          <ItemCard key={getItemId(item)} item={item} onPress={() => onItemPress(item)} />
        ))}
        {items.length === 0 && (
          <Text style={[styles.emptyColumnText, { color: c.textQuaternary }]}>No items</Text>
        )}
      </ScrollView>
    </View>
  );
}

export default function ViewsScreen() {
  const { state, setTasks, setIssues } = useApp();
  const theme = useTheme();
  const c = theme.colors;

  const [viewMode, setViewMode] = useState<ViewMode>('kanban');
  const [columns, setColumns] = useState<string[]>(DEFAULT_COLUMNS);
  const [configVisible, setConfigVisible] = useState(false);
  const [configInput, setConfigInput] = useState('');
  const [propertyTask, setPropertyTask] = useState<CalDavTask | null>(null);
  const [propertyIssue, setPropertyIssue] = useState<GiteaIssue | null>(null);

  // Assign modal state
  const [assignVisible, setAssignVisible] = useState(false);
  const [assignTarget, setAssignTarget] = useState<string>('');
  const [assigning, setAssigning] = useState<string | null>(null); // item id being assigned

  useEffect(() => {
    cache.loadKanbanColumns().then((saved) => {
      if (saved && saved.length > 0) setColumns(saved);
    });
  }, []);

  // Active tasks + open issues
  const activeTasks = useMemo(
    () => state.tasks.filter((t) => t.status !== 'COMPLETED' && t.status !== 'CANCELLED'),
    [state.tasks],
  );
  const openIssues = useMemo(
    () => state.issues.filter((i) => i.state === 'open'),
    [state.issues],
  );

  // All view items
  const allItems: ViewItem[] = useMemo(() => [
    ...activeTasks.map((task): ViewItem => ({ kind: 'task', task })),
    ...openIssues.map((issue): ViewItem => ({ kind: 'issue', issue })),
  ], [activeTasks, openIssues]);

  // Current view's tag set (for mutual exclusivity)
  const currentViewTags = useMemo(() => {
    if (viewMode === 'kanban') return columns.map((c) => c.toLowerCase());
    return QUADRANT_TAGS;
  }, [viewMode, columns]);

  // Get current tag within this view for an item (if any)
  function getCurrentViewTag(item: ViewItem): string | null {
    const tags = getItemTags(item);
    for (const t of tags) {
      if (currentViewTags.includes(t.toLowerCase())) return t;
    }
    return null;
  }

  // Kanban data: group items by column tag
  const kanbanData = useMemo(() => {
    return columns.map((col) => ({
      tag: col,
      items: allItems.filter((item) => itemHasTag(item, col)),
    }));
  }, [allItems, columns]);

  // Quadrant data: group items by quadrant tag
  const quadrantData = useMemo(() => {
    return QUADRANTS.map((q) => ({
      ...q,
      items: allItems.filter((item) => itemHasTag(item, q.tag)),
    }));
  }, [allItems]);

  // Items available for assignment: all items (user can reassign)
  const assignableItems = useMemo(() => {
    return allItems.filter((item) => {
      const current = getCurrentViewTag(item);
      // Show items that aren't already in the target
      return !current || current.toLowerCase() !== assignTarget.toLowerCase();
    });
  }, [allItems, assignTarget, currentViewTags]);

  function openConfig() {
    setConfigInput(columns.join(', '));
    setConfigVisible(true);
  }

  function saveConfig() {
    const parsed = configInput
      .split(',')
      .map((s) => s.trim().replace(/^#/, ''))
      .filter(Boolean);
    if (parsed.length > 0) {
      setColumns(parsed);
      cache.saveKanbanColumns(parsed);
    }
    setConfigVisible(false);
  }

  function openAssignModal(targetTag: string) {
    setAssignTarget(targetTag);
    setAssignVisible(true);
  }

  function handleItemPress(item: ViewItem) {
    if (item.kind === 'task') setPropertyTask(item.task);
    else setPropertyIssue(item.issue);
  }

  // Assign an item to a tag: remove conflicting view tags, add the new one, sync
  const assignItem = useCallback(async (item: ViewItem, targetTag: string) => {
    const itemId = getItemId(item);
    setAssigning(itemId);

    try {
      if (item.kind === 'task') {
        const task = item.task;
        const oldCategories = task.categories || [];
        // Remove any tags from the current view's tag set
        const cleaned = oldCategories.filter(
          (cat) => !currentViewTags.includes(cat.toLowerCase())
        );
        // Add the new tag
        cleaned.push(targetTag);
        const updated: CalDavTask = {
          ...task,
          categories: cleaned,
          lastModified: new Date(),
        };
        // Sync to CalDAV
        if (state.caldavConfigured) {
          await caldav.updateTask(updated);
        }
        // Update local state
        const newTasks = state.tasks.map((t) => (t.uid === task.uid ? updated : t));
        setTasks(newTasks);
        await cache.saveTasks(newTasks);
      } else {
        const issue = item.issue;
        const [owner, repo] = issue.repository.split('/');
        // Get existing labels, remove conflicting view tags
        const keptLabels = issue.labels.filter(
          (l) => !currentViewTags.includes(l.name.toLowerCase())
        );
        // Find or create the target label in the repo
        const repoLabels = await gitea.fetchLabels(owner, repo);
        let targetLabel = repoLabels.find(
          (l) => l.name.toLowerCase() === targetTag.toLowerCase()
        );
        if (!targetLabel) {
          targetLabel = (await gitea.createRepoLabel(owner, repo, targetTag)) ?? undefined;
        }
        if (targetLabel) {
          const newLabels = [...keptLabels, targetLabel];
          const labelIds = newLabels.map((l) => l.id);
          await gitea.replaceIssueLabels(owner, repo, issue.number, labelIds);
          // Update local state
          const updatedIssue: GiteaIssue = { ...issue, labels: newLabels, updatedAt: new Date() };
          const newIssues = state.issues.map((i) => (i.id === issue.id ? updatedIssue : i));
          setIssues(newIssues);
          await cache.saveIssues(newIssues);
        }
      }
    } catch (error) {
      console.error('Error assigning item:', error);
    } finally {
      setAssigning(null);
    }
  }, [state.tasks, state.issues, state.caldavConfigured, currentViewTags, setTasks, setIssues]);

  return (
    <View style={[styles.container, { backgroundColor: c.background }]}>
      {/* Header */}
      <View style={[styles.header, { backgroundColor: c.surface, borderBottomColor: c.border }]}>
        <Text style={[styles.headerTitle, { color: c.text }]}>Views</Text>
        <View style={styles.headerActions}>
          <TouchableOpacity
            style={[styles.addButton, { backgroundColor: c.primary }]}
            onPress={() => {
              const tags = viewMode === 'kanban' ? columns : QUADRANTS.map((q) => q.tag);
              openAssignModal(tags[0]);
            }}
          >
            <AppIcon name={Icons.add} size={16} color="#fff" />
            <Text style={styles.addButtonText}>Assign</Text>
          </TouchableOpacity>
          {viewMode === 'kanban' && (
            <TouchableOpacity style={styles.configButton} onPress={openConfig}>
              <AppIcon name={Icons.filter} size={18} color={c.primary} />
            </TouchableOpacity>
          )}
        </View>
      </View>

      {/* View toggle */}
      <View style={[styles.toggleBar, { borderBottomColor: c.border }]}>
        <TouchableOpacity
          style={[
            styles.toggleChip,
            { backgroundColor: viewMode === 'kanban' ? c.primary : c.filterChip },
          ]}
          onPress={() => setViewMode('kanban')}
        >
          <Text style={[styles.toggleChipText, { color: viewMode === 'kanban' ? '#fff' : c.textSecondary }]}>
            Kanban
          </Text>
        </TouchableOpacity>
        <TouchableOpacity
          style={[
            styles.toggleChip,
            { backgroundColor: viewMode === 'quadrants' ? c.primary : c.filterChip },
          ]}
          onPress={() => setViewMode('quadrants')}
        >
          <Text style={[styles.toggleChipText, { color: viewMode === 'quadrants' ? '#fff' : c.textSecondary }]}>
            Quadrants
          </Text>
        </TouchableOpacity>
      </View>

      {/* Kanban View */}
      {viewMode === 'kanban' && (
        <ScrollView horizontal style={styles.kanbanScroll} contentContainerStyle={styles.kanbanScrollContent}>
          {kanbanData.map((col, idx) => (
            <KanbanColumn
              key={col.tag}
              title={col.tag}
              color={COLUMN_COLORS[idx % COLUMN_COLORS.length]}
              items={col.items}
              onItemPress={handleItemPress}
              onAddPress={() => openAssignModal(col.tag)}
            />
          ))}
        </ScrollView>
      )}

      {/* Quadrants View */}
      {viewMode === 'quadrants' && (
        <View style={styles.quadrantsContainer}>
          {quadrantData.map((q) => (
            <View key={q.tag} style={[styles.quadrant, { borderColor: c.border }]}>
              <View style={[styles.quadrantHeader, { backgroundColor: q.color }]}>
                <View>
                  <Text style={styles.quadrantTitle}>{q.label}</Text>
                  <Text style={styles.quadrantSubtitle}>{q.subtitle}</Text>
                </View>
                <TouchableOpacity
                  onPress={() => openAssignModal(q.tag)}
                  style={styles.columnAddButton}
                >
                  <AppIcon name={Icons.add} size={16} color="#fff" />
                </TouchableOpacity>
              </View>
              <ScrollView style={styles.quadrantBody} contentContainerStyle={styles.quadrantContent}>
                {q.items.map((item) => (
                  <ItemCard key={getItemId(item)} item={item} onPress={() => handleItemPress(item)} />
                ))}
                {q.items.length === 0 && (
                  <Text style={[styles.emptyColumnText, { color: c.textQuaternary }]}>No items</Text>
                )}
              </ScrollView>
            </View>
          ))}
        </View>
      )}

      {/* Task property page */}
      {propertyTask && (
        <TaskPropertyPage
          task={propertyTask}
          allTasks={state.tasks}
          calendars={state.selectedCalendars}
          canEdit={false}
          onClose={() => setPropertyTask(null)}
          onSave={async () => {}}
          onDelete={async () => {}}
        />
      )}

      {/* Issue property page */}
      {propertyIssue && (
        <IssuePropertyPage
          issue={propertyIssue}
          canEdit={false}
          onClose={() => setPropertyIssue(null)}
          onSave={async () => {}}
          onStateToggle={async () => {}}
        />
      )}

      {/* Assign modal */}
      <Modal visible={assignVisible} animationType="fade" transparent>
        <View style={styles.modalOverlay}>
          <View style={[styles.assignModalContent, { backgroundColor: c.surface }]}>
            {/* Fixed header section */}
            <View style={styles.assignModalFixed}>
              <View style={styles.modalHeader}>
                <Text style={[styles.modalTitle, { color: c.text }]}>Assign Items</Text>
                <TouchableOpacity onPress={() => setAssignVisible(false)}>
                  <AppIcon name={Icons.close} size={22} color={c.textSecondary} />
                </TouchableOpacity>
              </View>

              {/* Target tag selector */}
              <Text style={[styles.assignLabel, { color: c.textSecondary }]}>Move to:</Text>
              <View style={styles.assignTargetRow}>
                {(viewMode === 'kanban' ? columns : QUADRANTS.map((q) => q.tag)).map((tag) => (
                  <TouchableOpacity
                    key={tag}
                    style={[
                      styles.assignTargetChip,
                      {
                        backgroundColor: assignTarget.toLowerCase() === tag.toLowerCase() ? c.primary : c.filterChip,
                      },
                    ]}
                    onPress={() => setAssignTarget(tag)}
                  >
                    <Text
                      style={[
                        styles.assignTargetChipText,
                        { color: assignTarget.toLowerCase() === tag.toLowerCase() ? '#fff' : c.textSecondary },
                      ]}
                    >
                      #{tag}
                    </Text>
                  </TouchableOpacity>
                ))}
              </View>
            </View>

            {/* Scrollable items list */}
            <ScrollView style={styles.assignList} contentContainerStyle={styles.assignListContent}>
              {assignableItems.length === 0 && (
                <Text style={[styles.emptyColumnText, { color: c.textQuaternary }]}>
                  All items are already in #{assignTarget}
                </Text>
              )}
              {assignableItems.map((item) => {
                const currentTag = getCurrentViewTag(item);
                const isAssigning = assigning === getItemId(item);
                return (
                  <TouchableOpacity
                    key={getItemId(item)}
                    style={[styles.assignItemRow, { borderBottomColor: c.border }]}
                    onPress={() => assignItem(item, assignTarget)}
                    disabled={isAssigning}
                  >
                    <AppIcon
                      name={item.kind === 'task' ? Icons.task : Icons.issues}
                      size={16}
                      color={c.textTertiary}
                    />
                    <View style={styles.assignItemContent}>
                      <Text style={[styles.assignItemTitle, { color: c.text }]} numberOfLines={1}>
                        {getItemTitle(item)}
                      </Text>
                      {currentTag && (
                        <Text style={[styles.assignItemSub, { color: c.textTertiary }]}>
                          currently: #{currentTag}
                        </Text>
                      )}
                    </View>
                    {isAssigning ? (
                      <ActivityIndicator size="small" color={c.primary} />
                    ) : (
                      <AppIcon name={Icons.chevronRight} size={18} color={c.textQuaternary} />
                    )}
                  </TouchableOpacity>
                );
              })}
            </ScrollView>
          </View>
        </View>
      </Modal>

      {/* Kanban config modal */}
      <Modal visible={configVisible} animationType="fade" transparent>
        <View style={styles.modalOverlay}>
          <View style={[styles.modalContent, { backgroundColor: c.surface }]}>
            <View style={styles.modalHeader}>
              <Text style={[styles.modalTitle, { color: c.text }]}>Kanban Columns</Text>
              <TouchableOpacity onPress={() => setConfigVisible(false)}>
                <AppIcon name={Icons.close} size={22} color={c.textSecondary} />
              </TouchableOpacity>
            </View>
            <Text style={[styles.modalHint, { color: c.textTertiary }]}>
              Enter tag names separated by commas (without #)
            </Text>
            <TextInput
              style={[styles.configInput, { borderColor: c.border, backgroundColor: c.inputBackground, color: c.text }]}
              value={configInput}
              onChangeText={setConfigInput}
              placeholder="backlog, planned, inprogress, done"
              placeholderTextColor={c.textTertiary}
              autoFocus
            />
            <View style={styles.modalActions}>
              <TouchableOpacity
                style={[styles.modalButton, { backgroundColor: c.filterChip }]}
                onPress={() => setConfigInput(DEFAULT_COLUMNS.join(', '))}
              >
                <Text style={[styles.modalButtonText, { color: c.textSecondary }]}>Reset</Text>
              </TouchableOpacity>
              <TouchableOpacity
                style={[styles.modalButton, { backgroundColor: c.primary }]}
                onPress={saveConfig}
              >
                <Text style={[styles.modalButtonText, { color: '#fff' }]}>Save</Text>
              </TouchableOpacity>
            </View>
          </View>
        </View>
      </Modal>
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
  headerTitle: {
    fontSize: 20,
    fontWeight: '600',
  },
  headerActions: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 12,
  },
  addButton: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 4,
    paddingHorizontal: 12,
    paddingVertical: 6,
    borderRadius: 8,
  },
  addButtonText: {
    color: '#fff',
    fontSize: 13,
    fontWeight: '600',
  },
  configButton: {
    padding: 4,
  },
  toggleBar: {
    flexDirection: 'row',
    padding: 12,
    paddingHorizontal: 16,
    gap: 8,
    borderBottomWidth: 1,
  },
  toggleChip: {
    paddingHorizontal: 14,
    paddingVertical: 6,
    borderRadius: 16,
  },
  toggleChipText: {
    fontSize: 13,
    fontWeight: '500',
  },
  // Kanban
  kanbanScroll: {
    flex: 1,
  },
  kanbanScrollContent: {
    padding: 16,
    gap: 12,
  },
  kanbanColumn: {
    width: 260,
    borderRadius: 10,
    borderWidth: 1,
    overflow: 'hidden',
    maxHeight: '100%',
  },
  kanbanColumnHeader: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    paddingHorizontal: 12,
    paddingVertical: 10,
  },
  kanbanColumnHeaderRight: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 8,
  },
  kanbanColumnTitle: {
    fontSize: 14,
    fontWeight: '700',
    color: '#fff',
  },
  kanbanColumnCount: {
    fontSize: 12,
    fontWeight: '600',
    color: 'rgba(255,255,255,0.8)',
  },
  columnAddButton: {
    width: 24,
    height: 24,
    borderRadius: 12,
    backgroundColor: 'rgba(255,255,255,0.25)',
    alignItems: 'center',
    justifyContent: 'center',
  },
  kanbanColumnBody: {
    flex: 1,
  },
  kanbanColumnContent: {
    padding: 8,
    gap: 6,
  },
  // Quadrants
  quadrantsContainer: {
    flex: 1,
    flexDirection: 'row',
    flexWrap: 'wrap',
  },
  quadrant: {
    width: '50%',
    height: '50%',
    borderWidth: 0.5,
    overflow: 'hidden',
  },
  quadrantHeader: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    paddingHorizontal: 12,
    paddingVertical: 8,
  },
  quadrantTitle: {
    fontSize: 14,
    fontWeight: '700',
    color: '#fff',
  },
  quadrantSubtitle: {
    fontSize: 11,
    color: 'rgba(255,255,255,0.8)',
    marginTop: 1,
  },
  quadrantBody: {
    flex: 1,
  },
  quadrantContent: {
    padding: 8,
    gap: 6,
  },
  // Item card
  taskCard: {
    padding: 10,
    borderRadius: 6,
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 1 },
    shadowOpacity: 0.08,
    shadowRadius: 2,
    elevation: 1,
  },
  taskCardRow: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 6,
  },
  priorityDot: {
    width: 7,
    height: 7,
    borderRadius: 4,
  },
  taskCardTitle: {
    fontSize: 13,
    fontWeight: '500',
    flex: 1,
  },
  taskCardTitleCompleted: {
    textDecorationLine: 'line-through',
    opacity: 0.6,
  },
  taskCardDue: {
    fontSize: 11,
    marginTop: 4,
  },
  emptyColumnText: {
    fontSize: 12,
    textAlign: 'center',
    paddingVertical: 16,
  },
  // Assign modal
  assignModalContent: {
    width: 440,
    maxWidth: '95%',
    height: WINDOW_HEIGHT * 0.75,
    borderRadius: 12,
    overflow: 'hidden',
    paddingBottom: 8,
  },
  assignModalFixed: {
    paddingHorizontal: 20,
    paddingTop: 20,
    paddingBottom: 4,
  },
  assignLabel: {
    fontSize: 13,
    fontWeight: '600',
    marginBottom: 8,
  },
  assignTargetRow: {
    flexDirection: 'row',
    flexWrap: 'wrap',
    gap: 6,
    marginBottom: 16,
  },
  assignTargetChip: {
    paddingHorizontal: 12,
    paddingVertical: 6,
    borderRadius: 16,
  },
  assignTargetChipText: {
    fontSize: 13,
    fontWeight: '500',
  },
  assignList: {
    flex: 1,
    paddingHorizontal: 20,
  },
  assignListContent: {
    paddingBottom: 16,
  },
  assignItemRow: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 10,
    paddingVertical: 10,
    borderBottomWidth: 1,
  },
  assignItemContent: {
    flex: 1,
  },
  assignItemTitle: {
    fontSize: 14,
    fontWeight: '500',
  },
  assignItemSub: {
    fontSize: 11,
    marginTop: 2,
  },
  // Config modal
  modalOverlay: {
    flex: 1,
    backgroundColor: 'rgba(0,0,0,0.5)',
    justifyContent: 'center',
    alignItems: 'center',
  },
  modalContent: {
    width: 380,
    maxWidth: '90%',
    borderRadius: 12,
    padding: 20,
  },
  modalHeader: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    marginBottom: 8,
  },
  modalTitle: {
    fontSize: 18,
    fontWeight: '600',
  },
  modalHint: {
    fontSize: 13,
    marginBottom: 12,
  },
  configInput: {
    borderWidth: 1,
    borderRadius: 8,
    padding: 12,
    fontSize: 15,
    marginBottom: 16,
  },
  modalActions: {
    flexDirection: 'row',
    justifyContent: 'flex-end',
    gap: 8,
  },
  modalButton: {
    paddingHorizontal: 16,
    paddingVertical: 8,
    borderRadius: 8,
  },
  modalButtonText: {
    fontSize: 14,
    fontWeight: '600',
  },
});
