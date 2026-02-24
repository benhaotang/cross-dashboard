import React, { useState, useEffect, useMemo, useCallback } from 'react';
import {
  View,
  Text,
  StyleSheet,
  FlatList,
  TouchableOpacity,
  TextInput,
  Modal,
  ActivityIndicator,
  ScrollView,
} from 'react-native';
import { useApp } from '../store/AppContext';
import { useTheme } from '../hooks/useTheme';
import { useSyncAll } from '../hooks/useSyncAll';
import * as cache from '../services/cache';
import * as caldav from '../services/caldav';
import * as keyring from '../services/keyring';
import { parseTaskInput, TaskDefaults, DEFAULT_TASK_DEFAULTS } from '../services/taskParser';
import { CalDavTask, CalDavCalendar, TaskStatus } from '../types';
import AppIcon, { Icons } from '../components/Icon';
import TaskPropertyPage from '../components/TaskPropertyPage';

type FilterMode = 'all' | 'active' | 'completed';

interface TaskTreeNode {
  task: CalDavTask;
  children: TaskTreeNode[];
  depth: number;
}

function buildTaskTree(tasks: CalDavTask[]): TaskTreeNode[] {
  const byUid = new Map<string, CalDavTask>();
  const childrenMap = new Map<string, CalDavTask[]>();

  for (const t of tasks) {
    byUid.set(t.uid, t);
  }

  for (const t of tasks) {
    if (t.parentUid && byUid.has(t.parentUid)) {
      const list = childrenMap.get(t.parentUid) || [];
      list.push(t);
      childrenMap.set(t.parentUid, list);
    }
  }

  const roots = tasks.filter((t) => !t.parentUid || !byUid.has(t.parentUid));

  function toNode(task: CalDavTask, depth: number): TaskTreeNode {
    const kids = childrenMap.get(task.uid) || [];
    return {
      task,
      children: kids.map((k) => toNode(k, depth + 1)),
      depth,
    };
  }

  return roots.map((r) => toNode(r, 0));
}

function flattenTree(nodes: TaskTreeNode[], expanded: Set<string>): TaskTreeNode[] {
  const result: TaskTreeNode[] = [];
  for (const node of nodes) {
    result.push(node);
    if (node.children.length > 0 && expanded.has(node.task.uid)) {
      result.push(...flattenTree(node.children, expanded));
    }
  }
  return result;
}

function getPriorityColor(priority: number): string | null {
  if (priority >= 1 && priority <= 4) return '#F44336';
  if (priority === 5) return '#FF9800';
  if (priority >= 6 && priority <= 9) return '#2196F3';
  return null;
}

function getPriorityLabel(priority: number): string {
  if (priority >= 1 && priority <= 4) return 'High';
  if (priority === 5) return 'Medium';
  if (priority >= 6 && priority <= 9) return 'Low';
  return 'None';
}

function priorityFromLabel(label: string): number {
  if (label === 'High') return 1;
  if (label === 'Medium') return 5;
  if (label === 'Low') return 9;
  return 0;
}

function formatDuePreview(date: Date): string {
  const now = new Date();
  const today = new Date(now.getFullYear(), now.getMonth(), now.getDate());
  const tomorrow = new Date(today);
  tomorrow.setDate(tomorrow.getDate() + 1);
  const taskDay = new Date(date.getFullYear(), date.getMonth(), date.getDate());

  const time = date.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' });

  if (taskDay.getTime() === today.getTime()) {
    return `Today ${time}`;
  }
  if (taskDay.getTime() === tomorrow.getTime()) {
    return `Tomorrow ${time}`;
  }
  return `${date.toLocaleDateString([], { weekday: 'short', month: 'short', day: 'numeric' })} ${time}`;
}

const STATUS_OPTIONS: { label: string; value: TaskStatus }[] = [
  { label: 'Needs Action', value: 'NEEDS-ACTION' },
  { label: 'In Progress', value: 'IN-PROCESS' },
  { label: 'Completed', value: 'COMPLETED' },
  { label: 'Cancelled', value: 'CANCELLED' },
];

const PRIORITY_OPTIONS = ['None', 'Low', 'Medium', 'High'];

function useCalendarColorMap(calendars: CalDavCalendar[]): Map<string, string> {
  return React.useMemo(() => {
    const map = new Map<string, string>();
    for (const cal of calendars) {
      if (cal.color) map.set(cal.href, cal.color);
    }
    return map;
  }, [calendars]);
}

export default function TasksScreen() {
  const { state, setTasks } = useApp();
  const theme = useTheme();
  const { syncAll } = useSyncAll();
  const calColorMap = useCalendarColorMap(state.selectedCalendars);
  const [modalVisible, setModalVisible] = useState(false);
  const [syncing, setSyncing] = useState(false);
  const [filter, setFilter] = useState<FilterMode>('all');
  const [expanded, setExpanded] = useState<Set<string>>(new Set());
  const [propertyTask, setPropertyTask] = useState<CalDavTask | null>(null);

  // Quick input state
  const [quickInput, setQuickInput] = useState('');
  const [taskDefaults, setTaskDefaults] = useState<TaskDefaults>(DEFAULT_TASK_DEFAULTS);
  const [defaultTaskCal, setDefaultTaskCal] = useState<string | null>(null);

  // Modal form state
  const [editTask, setEditTask] = useState<CalDavTask | null>(null);
  const [formSummary, setFormSummary] = useState('');
  const [formDescription, setFormDescription] = useState('');
  const [formStatus, setFormStatus] = useState<TaskStatus>('NEEDS-ACTION');
  const [formPriority, setFormPriority] = useState('None');
  const [formDue, setFormDue] = useState('');
  const [formPercent, setFormPercent] = useState('0');
  const [formParentUid, setFormParentUid] = useState('');

  useEffect(() => {
    loadDefaults();
    if (state.caldavConfigured && state.tasks.length === 0) {
      syncTasks();
    }
  }, [state.caldavConfigured]);

  async function loadDefaults() {
    const [saved, defCal] = await Promise.all([
      cache.loadTaskDefaults(),
      keyring.getCredential('caldav_default_task_calendar'),
    ]);
    if (saved) setTaskDefaults(saved);
    if (defCal) setDefaultTaskCal(defCal);
  }

  // Live parse preview
  const parsedPreview = useMemo(() => {
    if (!quickInput.trim()) return null;
    return parseTaskInput(quickInput, taskDefaults);
  }, [quickInput, taskDefaults]);

  function getTaskCalendarHrefs(): string[] | undefined {
    const hrefs = state.selectedCalendars
      .filter((c: CalDavCalendar) => c.components.includes('VTODO'))
      .map((c: CalDavCalendar) => c.href);
    return hrefs.length > 0 ? hrefs : undefined;
  }

  function getDefaultTaskCalHref(): string | undefined {
    if (defaultTaskCal) return defaultTaskCal;
    return getTaskCalendarHrefs()?.[0];
  }

  async function syncTasks() {
    setSyncing(true);
    try {
      const tasks = await caldav.fetchTasks(getTaskCalendarHrefs());
      if (tasks.length > 0 || state.tasks.length > 0) {
        setTasks(tasks);
        await cache.saveTasks(tasks);
      }
    } catch (error) {
      console.error('Error syncing tasks:', error);
    } finally {
      setSyncing(false);
    }
  }

  const filteredTasks = useMemo(() => {
    if (filter === 'active') {
      return state.tasks.filter((t) => t.status !== 'COMPLETED' && t.status !== 'CANCELLED');
    }
    if (filter === 'completed') {
      return state.tasks.filter((t) => t.status === 'COMPLETED');
    }
    return state.tasks;
  }, [state.tasks, filter]);

  const tree = useMemo(() => buildTaskTree(filteredTasks), [filteredTasks]);
  const flatList = useMemo(() => flattenTree(tree, expanded), [tree, expanded]);

  function toggleExpand(uid: string) {
    setExpanded((prev) => {
      const next = new Set(prev);
      if (next.has(uid)) next.delete(uid);
      else next.add(uid);
      return next;
    });
  }

  // Quick input submit
  const submitQuickInput = useCallback(async () => {
    if (!quickInput.trim()) return;
    const parsed = parseTaskInput(quickInput, taskDefaults);
    if (!parsed.summary.trim()) return;

    const now = new Date();
    const taskData = {
      summary: parsed.summary,
      description: undefined as string | undefined,
      status: 'NEEDS-ACTION' as TaskStatus,
      priority: parsed.priority,
      percentComplete: 0,
      due: parsed.due,
      dtstart: undefined as Date | undefined,
      completed: undefined as Date | undefined,
      categories: parsed.categories.length > 0 ? parsed.categories : undefined,
      location: undefined as string | undefined,
      parentUid: undefined as string | undefined,
    };

    const taskCalHref = getDefaultTaskCalHref();
    let newTask: CalDavTask;
    if (state.caldavConfigured) {
      const created = await caldav.createTask(taskData, taskCalHref);
      newTask = created || { ...taskData, uid: `task-${Date.now()}@cross-dashboard`, created: now, lastModified: now };
    } else {
      newTask = { ...taskData, uid: `task-${Date.now()}@cross-dashboard`, created: now, lastModified: now };
    }

    const newTasks = [newTask, ...state.tasks];
    setTasks(newTasks);
    await cache.saveTasks(newTasks);
    setQuickInput('');
  }, [quickInput, taskDefaults, state.caldavConfigured, state.selectedCalendars, state.tasks, setTasks, defaultTaskCal]);

  function openNewTask() {
    setEditTask(null);
    setFormSummary('');
    setFormDescription('');
    setFormStatus('NEEDS-ACTION');
    setFormPriority('None');
    setFormDue('');
    setFormPercent('0');
    setFormParentUid('');
    setModalVisible(true);
  }

  async function saveTask() {
    if (!formSummary.trim()) return;

    const now = new Date();
    const priority = priorityFromLabel(formPriority);
    const percent = Math.max(0, Math.min(100, parseInt(formPercent, 10) || 0));
    const due = formDue ? new Date(formDue + 'T00:00:00') : undefined;
    const isCompleted = formStatus === 'COMPLETED';

    if (editTask) {
      const updated: CalDavTask = {
        ...editTask,
        summary: formSummary,
        description: formDescription || undefined,
        status: formStatus,
        priority,
        percentComplete: isCompleted ? 100 : percent,
        due,
        completed: isCompleted ? (editTask.completed || now) : undefined,
        lastModified: now,
        parentUid: formParentUid || undefined,
      };

      if (state.caldavConfigured) {
        await caldav.updateTask(updated);
      }

      const newTasks = state.tasks.map((t) => (t.uid === editTask.uid ? updated : t));
      setTasks(newTasks);
      await cache.saveTasks(newTasks);
      if (isCompleted && editTask.status !== 'COMPLETED') cache.incrementStat('tasksCompleted');
    } else {
      const taskData = {
        summary: formSummary,
        description: formDescription || undefined,
        status: formStatus,
        priority,
        percentComplete: isCompleted ? 100 : percent,
        due,
        dtstart: undefined as Date | undefined,
        completed: isCompleted ? now : undefined,
        categories: undefined as string[] | undefined,
        location: undefined as string | undefined,
        parentUid: formParentUid || undefined,
      };

      const taskCalHref = getDefaultTaskCalHref();
      let newTask: CalDavTask;
      if (state.caldavConfigured) {
        const created = await caldav.createTask(taskData, taskCalHref);
        newTask = created || { ...taskData, uid: `task-${Date.now()}@cross-dashboard`, created: now, lastModified: now };
      } else {
        newTask = { ...taskData, uid: `task-${Date.now()}@cross-dashboard`, created: now, lastModified: now };
      }

      const newTasks = [newTask, ...state.tasks];
      setTasks(newTasks);
      await cache.saveTasks(newTasks);
    }

    setModalVisible(false);
  }

  async function toggleCompletion(task: CalDavTask) {
    const now = new Date();
    const isCompleting = task.status !== 'COMPLETED';
    const updated: CalDavTask = {
      ...task,
      status: isCompleting ? 'COMPLETED' : 'NEEDS-ACTION',
      percentComplete: isCompleting ? 100 : 0,
      completed: isCompleting ? now : undefined,
      lastModified: now,
    };

    if (state.caldavConfigured) {
      await caldav.updateTask(updated);
    }

    const newTasks = state.tasks.map((t) => (t.uid === task.uid ? updated : t));
    setTasks(newTasks);
    await cache.saveTasks(newTasks);
    if (isCompleting) cache.incrementStat('tasksCompleted');
  }

  async function deleteTask(uid: string) {
    const task = state.tasks.find((t) => t.uid === uid);
    if (state.caldavConfigured) {
      await caldav.deleteTask(uid, task?.calendarHref);
    }
    const newTasks = state.tasks.filter((t) => t.uid !== uid);
    setTasks(newTasks);
    await cache.saveTasks(newTasks);
  }

  const c = theme.colors;

  function renderTaskItem({ item }: { item: TaskTreeNode }) {
    const { task, children, depth } = item;
    const isCompleted = task.status === 'COMPLETED';
    const priorityColor = getPriorityColor(task.priority);
    const isOverdue = task.due && task.due < new Date() && !isCompleted;
    const childCount = children.length;
    const isExpanded = expanded.has(task.uid);

    return (
      <TouchableOpacity
        style={[styles.taskCard, { backgroundColor: c.surface, marginLeft: depth * 24 }]}
        onPress={() => setPropertyTask(task)}
      >
        <View style={styles.taskRow}>
          <TouchableOpacity onPress={() => toggleCompletion(task)} style={styles.checkbox}>
            <AppIcon
              name={isCompleted ? Icons.task : Icons.taskOutline}
              size={22}
              color={isCompleted ? '#4CAF50' : c.textTertiary}
            />
          </TouchableOpacity>

          <View style={styles.taskContent}>
            <View style={styles.taskTitleRow}>
              {task.calendarHref && calColorMap.get(task.calendarHref) && (
                <View style={[styles.taskCalDot, { backgroundColor: calColorMap.get(task.calendarHref) }]} />
              )}
              {priorityColor && (
                <View style={[styles.priorityDot, { backgroundColor: priorityColor }]} />
              )}
              <Text
                style={[
                  styles.taskTitle,
                  { color: c.text },
                  isCompleted && styles.taskTitleCompleted,
                ]}
                numberOfLines={1}
              >
                {task.summary}
              </Text>
            </View>

            <View style={styles.taskMeta}>
              {task.due && (
                <Text style={[styles.taskDue, isOverdue && styles.taskOverdue]}>
                  {task.due.toLocaleDateString([], { month: 'short', day: 'numeric' })}
                </Text>
              )}
              {task.categories && task.categories.length > 0 && (
                <View style={styles.tagRow}>
                  {task.categories.map((cat) => (
                    <View key={cat} style={[styles.tagChipSmall, { backgroundColor: '#E3F2FD' }]}>
                      <Text style={styles.tagChipSmallText}>#{cat}</Text>
                    </View>
                  ))}
                </View>
              )}
              {task.percentComplete > 0 && task.percentComplete < 100 && (
                <View style={styles.progressContainer}>
                  <View style={styles.progressBar}>
                    <View style={[styles.progressFill, { width: `${task.percentComplete}%` }]} />
                  </View>
                  <Text style={[styles.progressText, { color: c.textTertiary }]}>{task.percentComplete}%</Text>
                </View>
              )}
            </View>
          </View>

          <View style={styles.taskActions}>
            {childCount > 0 && (
              <TouchableOpacity onPress={() => toggleExpand(task.uid)} style={styles.expandButton}>
                <AppIcon name={Icons.subtask} size={16} color={c.textTertiary} />
                <Text style={[styles.childCount, { color: c.textTertiary }]}>{childCount}</Text>
                <AppIcon
                  name={isExpanded ? 'mdi:chevron-down' : Icons.chevronRight}
                  size={16}
                  color={c.textTertiary}
                />
              </TouchableOpacity>
            )}
            <TouchableOpacity onPress={() => deleteTask(task.uid)} style={styles.deleteButton}>
              <AppIcon name={Icons.delete} size={18} color="#F44336" />
            </TouchableOpacity>
          </View>
        </View>
      </TouchableOpacity>
    );
  }

  // Available parent tasks for the dropdown (exclude self and descendants)
  const availableParents = useMemo(() => {
    if (!editTask) return state.tasks;
    const excludeUids = new Set<string>();
    function collectDescendants(uid: string) {
      excludeUids.add(uid);
      for (const t of state.tasks) {
        if (t.parentUid === uid) collectDescendants(t.uid);
      }
    }
    collectDescendants(editTask.uid);
    return state.tasks.filter((t) => !excludeUids.has(t.uid));
  }, [state.tasks, editTask]);

  return (
    <View style={[styles.container, { backgroundColor: c.background }]}>
      <View style={[styles.header, { backgroundColor: c.surface, borderBottomColor: c.border }]}>
        <Text style={[styles.headerTitle, { color: c.text }]}>Tasks</Text>
        <View style={styles.headerActions}>
          {state.caldavConfigured && (
            <TouchableOpacity style={styles.syncButton} onPress={syncAll}>
              <AppIcon name={Icons.refresh} size={18} color={c.primary} />
            </TouchableOpacity>
          )}
          <TouchableOpacity style={[styles.addButton, { backgroundColor: c.primary }]} onPress={openNewTask}>
            <AppIcon name={Icons.add} size={18} color="#fff" />
            <Text style={styles.addButtonText}>New</Text>
          </TouchableOpacity>
        </View>
      </View>

      {/* Quick Input Bar */}
      <View style={[styles.quickInputContainer, { backgroundColor: c.surface, borderBottomColor: c.border }]}>
        <View style={styles.quickInputRow}>
          <TextInput
            style={[styles.quickInputField, { borderColor: c.border, backgroundColor: c.inputBackground, color: c.text }]}
            value={quickInput}
            onChangeText={setQuickInput}
            placeholder="!! task name #tag tonight"
            placeholderTextColor={c.textTertiary}
            onSubmitEditing={submitQuickInput}
            returnKeyType="done"
          />
          <TouchableOpacity
            style={[styles.quickAddButton, { backgroundColor: c.primary, opacity: quickInput.trim() ? 1 : 0.5 }]}
            onPress={submitQuickInput}
            disabled={!quickInput.trim()}
          >
            <AppIcon name={Icons.add} size={20} color="#fff" />
          </TouchableOpacity>
        </View>
        {parsedPreview && (
          <View style={styles.previewRow}>
            {parsedPreview.priority > 0 && (
              <View style={[styles.previewChip, { backgroundColor: getPriorityColor(parsedPreview.priority) || '#999' }]}>
                <Text style={styles.previewChipText}>{getPriorityLabel(parsedPreview.priority)}</Text>
              </View>
            )}
            {parsedPreview.categories.map((cat) => (
              <View key={cat} style={[styles.previewChip, { backgroundColor: '#2196F3' }]}>
                <Text style={styles.previewChipText}>#{cat}</Text>
              </View>
            ))}
            {parsedPreview.due && (
              <View style={[styles.previewChip, { backgroundColor: '#4CAF50' }]}>
                <AppIcon name={Icons.time} size={12} color="#fff" />
                <Text style={styles.previewChipText}>{formatDuePreview(parsedPreview.due)}</Text>
              </View>
            )}
            {parsedPreview.summary && (
              <Text style={[styles.previewSummary, { color: c.textSecondary }]} numberOfLines={1}>
                {parsedPreview.summary}
              </Text>
            )}
          </View>
        )}
      </View>

      <View style={[styles.filterBar, { borderBottomColor: c.border }]}>
        {(['all', 'active', 'completed'] as FilterMode[]).map((mode) => (
          <TouchableOpacity
            key={mode}
            style={[
              styles.filterChip,
              { backgroundColor: filter === mode ? c.primary : c.filterChip },
            ]}
            onPress={() => setFilter(mode)}
          >
            <Text style={[styles.filterChipText, { color: filter === mode ? '#fff' : c.textSecondary }]}>
              {mode.charAt(0).toUpperCase() + mode.slice(1)}
            </Text>
          </TouchableOpacity>
        ))}
      </View>

      {syncing && <ActivityIndicator size="small" color={c.primary} style={{ marginTop: 12 }} />}

      <FlatList
        data={flatList}
        renderItem={renderTaskItem}
        keyExtractor={(item) => item.task.uid}
        contentContainerStyle={styles.listContent}
        ListEmptyComponent={
          <View style={styles.emptyContainer}>
            <AppIcon name={Icons.task} size={48} color={c.textQuaternary} />
            <Text style={[styles.emptyText, { color: c.textTertiary }]}>No tasks yet</Text>
            <Text style={[styles.hintText, { color: c.textQuaternary }]}>
              Type above to quickly add a task, e.g. "!! buy milk #errands tomorrow"
            </Text>
          </View>
        }
      />

      {propertyTask && (
        <TaskPropertyPage
          task={propertyTask}
          allTasks={state.tasks}
          calendars={state.selectedCalendars}
          canEdit
          onClose={() => setPropertyTask(null)}
          onSave={async (updated) => {
            if (state.caldavConfigured) {
              await caldav.updateTask(updated);
            }
            const newTasks = state.tasks.map((t) => (t.uid === updated.uid ? updated : t));
            setTasks(newTasks);
            await cache.saveTasks(newTasks);
            if (updated.status === 'COMPLETED' && propertyTask?.status !== 'COMPLETED') {
              cache.incrementStat('tasksCompleted');
            }
            setPropertyTask(null);
          }}
          onDelete={async (uid) => {
            await deleteTask(uid);
            setPropertyTask(null);
          }}
        />
      )}

      <Modal visible={modalVisible} animationType="slide" transparent>
        <View style={styles.modalOverlay}>
          <ScrollView style={[styles.modalContent, { backgroundColor: c.surface }]} keyboardShouldPersistTaps="handled">
            <View style={styles.modalHeader}>
              <Text style={[styles.modalTitle, { color: c.text }]}>{editTask ? 'Edit Task' : 'New Task'}</Text>
              <TouchableOpacity onPress={() => setModalVisible(false)}>
                <Text style={[styles.cancelText, { color: c.primary }]}>Cancel</Text>
              </TouchableOpacity>
            </View>

            <TextInput
              style={[styles.input, { borderColor: c.border, backgroundColor: c.inputBackground, color: c.text }]}
              value={formSummary}
              onChangeText={setFormSummary}
              placeholder="Task summary"
              placeholderTextColor={c.textTertiary}
              autoFocus
            />

            <TextInput
              style={[styles.input, styles.multilineInput, { borderColor: c.border, backgroundColor: c.inputBackground, color: c.text }]}
              value={formDescription}
              onChangeText={setFormDescription}
              placeholder="Description (optional)"
              placeholderTextColor={c.textTertiary}
              multiline
              textAlignVertical="top"
            />

            <Text style={[styles.fieldLabel, { color: c.textSecondary }]}>Status</Text>
            <View style={styles.segmentedRow}>
              {STATUS_OPTIONS.map((opt) => (
                <TouchableOpacity
                  key={opt.value}
                  style={[
                    styles.segmentButton,
                    { borderColor: c.border },
                    formStatus === opt.value && { backgroundColor: c.primary, borderColor: c.primary },
                  ]}
                  onPress={() => setFormStatus(opt.value)}
                >
                  <Text style={[styles.segmentText, formStatus === opt.value && { color: '#fff' }]}>
                    {opt.label}
                  </Text>
                </TouchableOpacity>
              ))}
            </View>

            <Text style={[styles.fieldLabel, { color: c.textSecondary }]}>Priority</Text>
            <View style={styles.segmentedRow}>
              {PRIORITY_OPTIONS.map((opt) => (
                <TouchableOpacity
                  key={opt}
                  style={[
                    styles.segmentButton,
                    { borderColor: c.border },
                    formPriority === opt && { backgroundColor: c.primary, borderColor: c.primary },
                  ]}
                  onPress={() => setFormPriority(opt)}
                >
                  <Text style={[styles.segmentText, formPriority === opt && { color: '#fff' }]}>
                    {opt}
                  </Text>
                </TouchableOpacity>
              ))}
            </View>

            <View style={styles.inlineRow}>
              <View style={styles.inlineField}>
                <Text style={[styles.fieldLabel, { color: c.textSecondary }]}>Due Date</Text>
                <TextInput
                  style={[styles.input, { borderColor: c.border, backgroundColor: c.inputBackground, color: c.text }]}
                  value={formDue}
                  onChangeText={setFormDue}
                  placeholder="YYYY-MM-DD"
                  placeholderTextColor={c.textTertiary}
                />
              </View>
              <View style={styles.inlineField}>
                <Text style={[styles.fieldLabel, { color: c.textSecondary }]}>% Complete</Text>
                <TextInput
                  style={[styles.input, { borderColor: c.border, backgroundColor: c.inputBackground, color: c.text }]}
                  value={formPercent}
                  onChangeText={setFormPercent}
                  placeholder="0-100"
                  placeholderTextColor={c.textTertiary}
                  keyboardType="numeric"
                />
              </View>
            </View>

            <Text style={[styles.fieldLabel, { color: c.textSecondary }]}>Parent Task</Text>
            <View style={styles.parentPicker}>
              <TouchableOpacity
                style={[
                  styles.parentOption,
                  { borderColor: c.border },
                  !formParentUid && { backgroundColor: c.primary, borderColor: c.primary },
                ]}
                onPress={() => setFormParentUid('')}
              >
                <Text style={[styles.parentOptionText, !formParentUid && { color: '#fff' }]}>None</Text>
              </TouchableOpacity>
              {availableParents.slice(0, 5).map((t) => (
                <TouchableOpacity
                  key={t.uid}
                  style={[
                    styles.parentOption,
                    { borderColor: c.border },
                    formParentUid === t.uid && { backgroundColor: c.primary, borderColor: c.primary },
                  ]}
                  onPress={() => setFormParentUid(t.uid)}
                >
                  <Text
                    style={[styles.parentOptionText, formParentUid === t.uid && { color: '#fff' }]}
                    numberOfLines={1}
                  >
                    {t.summary}
                  </Text>
                </TouchableOpacity>
              ))}
            </View>

            <TouchableOpacity style={[styles.saveButton, { backgroundColor: c.primary }]} onPress={saveTask}>
              <Text style={styles.saveButtonText}>Save</Text>
            </TouchableOpacity>
          </ScrollView>
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
  syncButton: {
    padding: 4,
  },
  addButton: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 6,
    paddingHorizontal: 16,
    paddingVertical: 8,
    borderRadius: 8,
  },
  addButtonText: {
    color: '#fff',
    fontWeight: '600',
  },
  // Quick input
  quickInputContainer: {
    padding: 12,
    paddingHorizontal: 16,
    borderBottomWidth: 1,
  },
  quickInputRow: {
    flexDirection: 'row',
    gap: 8,
  },
  quickInputField: {
    flex: 1,
    borderWidth: 1,
    borderRadius: 8,
    paddingHorizontal: 12,
    paddingVertical: 10,
    fontSize: 15,
  },
  quickAddButton: {
    width: 42,
    height: 42,
    borderRadius: 8,
    alignItems: 'center',
    justifyContent: 'center',
  },
  previewRow: {
    flexDirection: 'row',
    flexWrap: 'wrap',
    alignItems: 'center',
    gap: 6,
    marginTop: 8,
  },
  previewChip: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 4,
    paddingHorizontal: 8,
    paddingVertical: 3,
    borderRadius: 12,
  },
  previewChipText: {
    fontSize: 11,
    color: '#fff',
    fontWeight: '600',
  },
  previewSummary: {
    fontSize: 12,
    flex: 1,
  },
  // Filter bar
  filterBar: {
    flexDirection: 'row',
    padding: 12,
    paddingHorizontal: 16,
    gap: 8,
    borderBottomWidth: 1,
  },
  filterChip: {
    paddingHorizontal: 14,
    paddingVertical: 6,
    borderRadius: 16,
  },
  filterChipText: {
    fontSize: 13,
    fontWeight: '500',
  },
  listContent: {
    padding: 16,
    flexGrow: 1,
  },
  taskCard: {
    padding: 12,
    borderRadius: 8,
    marginBottom: 8,
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 1 },
    shadowOpacity: 0.1,
    shadowRadius: 2,
    elevation: 2,
  },
  taskRow: {
    flexDirection: 'row',
    alignItems: 'center',
  },
  checkbox: {
    padding: 4,
    marginRight: 8,
  },
  taskContent: {
    flex: 1,
  },
  taskTitleRow: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 6,
  },
  taskCalDot: {
    width: 10,
    height: 10,
    borderRadius: 5,
  },
  priorityDot: {
    width: 8,
    height: 8,
    borderRadius: 4,
  },
  taskTitle: {
    fontSize: 15,
    fontWeight: '500',
    flex: 1,
  },
  taskTitleCompleted: {
    textDecorationLine: 'line-through',
    opacity: 0.6,
  },
  taskMeta: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 12,
    marginTop: 4,
  },
  taskDue: {
    fontSize: 12,
    color: '#666',
  },
  taskOverdue: {
    color: '#F44336',
    fontWeight: '600',
  },
  tagRow: {
    flexDirection: 'row',
    gap: 4,
  },
  tagChipSmall: {
    paddingHorizontal: 6,
    paddingVertical: 1,
    borderRadius: 8,
  },
  tagChipSmallText: {
    fontSize: 10,
    color: '#1565C0',
    fontWeight: '500',
  },
  progressContainer: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 4,
  },
  progressBar: {
    width: 60,
    height: 4,
    backgroundColor: '#E0E0E0',
    borderRadius: 2,
  },
  progressFill: {
    height: 4,
    backgroundColor: '#4CAF50',
    borderRadius: 2,
  },
  progressText: {
    fontSize: 11,
  },
  taskActions: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 8,
  },
  expandButton: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 2,
    padding: 4,
  },
  childCount: {
    fontSize: 12,
  },
  deleteButton: {
    padding: 4,
  },
  emptyContainer: {
    flex: 1,
    justifyContent: 'center',
    alignItems: 'center',
    paddingTop: 100,
  },
  emptyText: {
    fontSize: 16,
    marginTop: 16,
    marginBottom: 8,
  },
  hintText: {
    fontSize: 14,
    textAlign: 'center',
    paddingHorizontal: 32,
  },
  modalOverlay: {
    flex: 1,
    backgroundColor: 'rgba(0,0,0,0.5)',
    justifyContent: 'flex-end',
  },
  modalContent: {
    borderTopLeftRadius: 16,
    borderTopRightRadius: 16,
    padding: 20,
    maxHeight: '85%',
  },
  modalHeader: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    marginBottom: 16,
  },
  modalTitle: {
    fontSize: 18,
    fontWeight: '600',
  },
  cancelText: {
    fontSize: 16,
  },
  input: {
    borderWidth: 1,
    borderRadius: 8,
    padding: 12,
    fontSize: 15,
    marginBottom: 12,
  },
  multilineInput: {
    minHeight: 80,
  },
  fieldLabel: {
    fontSize: 13,
    fontWeight: '600',
    marginBottom: 6,
  },
  segmentedRow: {
    flexDirection: 'row',
    flexWrap: 'wrap',
    gap: 6,
    marginBottom: 12,
  },
  segmentButton: {
    borderWidth: 1,
    borderRadius: 6,
    paddingHorizontal: 10,
    paddingVertical: 6,
  },
  segmentText: {
    fontSize: 12,
    fontWeight: '500',
  },
  inlineRow: {
    flexDirection: 'row',
    gap: 12,
  },
  inlineField: {
    flex: 1,
  },
  parentPicker: {
    flexDirection: 'row',
    flexWrap: 'wrap',
    gap: 6,
    marginBottom: 12,
  },
  parentOption: {
    borderWidth: 1,
    borderRadius: 6,
    paddingHorizontal: 10,
    paddingVertical: 6,
    maxWidth: 160,
  },
  parentOptionText: {
    fontSize: 12,
  },
  saveButton: {
    padding: 14,
    borderRadius: 8,
    alignItems: 'center',
    marginTop: 8,
    marginBottom: 20,
  },
  saveButtonText: {
    color: '#fff',
    fontSize: 16,
    fontWeight: '600',
  },
});
