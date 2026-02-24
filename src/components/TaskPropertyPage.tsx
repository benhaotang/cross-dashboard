import React, { useState, useMemo, useCallback, useEffect } from 'react';
import {
  View,
  Text,
  StyleSheet,
  ScrollView,
  TextInput,
  TouchableOpacity,
} from 'react-native';
import { useTheme } from '../hooks/useTheme';
import { CalDavTask, CalDavCalendar, TaskStatus } from '../types';
import AppIcon, { Icons } from './Icon';
import { usePomodoro } from '../store/PomodoroContext';
import * as cache from '../services/cache';
import {
  PropertyPageModal,
  PropertyPageHeader,
  ReadField,
  SectionHeader,
  resolveCalendar,
} from './PropertyPageShared';

const QUADRANT_QUICK_TAGS = ['do', 'delay', 'delegate', 'eliminate'];
const TIME_QUICK_TAGS = ['5m', '30m', '1h', '2h'];

interface Props {
  task: CalDavTask;
  allTasks: CalDavTask[];
  calendars: CalDavCalendar[];
  onClose: () => void;
  onSave: (updated: CalDavTask) => void;
  onDelete: (uid: string) => void;
  canEdit: boolean;
}

const STATUS_OPTIONS: { label: string; value: TaskStatus }[] = [
  { label: 'Needs Action', value: 'NEEDS-ACTION' },
  { label: 'In Progress', value: 'IN-PROCESS' },
  { label: 'Completed', value: 'COMPLETED' },
  { label: 'Cancelled', value: 'CANCELLED' },
];

const PRIORITY_OPTIONS = ['None', 'Low', 'Medium', 'High'];

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

function getStatusColor(status: TaskStatus): string {
  switch (status) {
    case 'NEEDS-ACTION': return '#FF9800';
    case 'IN-PROCESS': return '#2196F3';
    case 'COMPLETED': return '#4CAF50';
    case 'CANCELLED': return '#9E9E9E';
  }
}

function getStatusLabel(status: TaskStatus): string {
  switch (status) {
    case 'NEEDS-ACTION': return 'Needs Action';
    case 'IN-PROCESS': return 'In Progress';
    case 'COMPLETED': return 'Completed';
    case 'CANCELLED': return 'Cancelled';
  }
}

export default function TaskPropertyPage({ task, allTasks, calendars, onClose, onSave, onDelete, canEdit }: Props) {
  const theme = useTheme();
  const c = theme.colors;
  const pomodoro = usePomodoro();
  const [editing, setEditing] = useState(false);
  const [kanbanColumns, setKanbanColumns] = useState<string[]>([]);

  useEffect(() => {
    cache.loadKanbanColumns().then((cols) => {
      if (cols && cols.length > 0) setKanbanColumns(cols);
      else setKanbanColumns(['backlog', 'planned', 'inprogress', 'done']);
    });
  }, []);

  // Form state
  const [formSummary, setFormSummary] = useState(task.summary);
  const [formDescription, setFormDescription] = useState(task.description || '');
  const [formStatus, setFormStatus] = useState<TaskStatus>(task.status);
  const [formPriority, setFormPriority] = useState(getPriorityLabel(task.priority));
  const [formDue, setFormDue] = useState(task.due ? task.due.toISOString().split('T')[0] : '');
  const [formPercent, setFormPercent] = useState(String(task.percentComplete));
  const [formParentUid, setFormParentUid] = useState(task.parentUid || '');
  const [formLocation, setFormLocation] = useState(task.location || '');
  const [formCategories, setFormCategories] = useState((task.categories || []).join(', '));

  const subtasks = useMemo(
    () => allTasks.filter((t) => t.parentUid === task.uid),
    [allTasks, task.uid]
  );

  const availableParents = useMemo(() => {
    const excludeUids = new Set<string>();
    function collectDescendants(uid: string) {
      excludeUids.add(uid);
      for (const t of allTasks) {
        if (t.parentUid === uid) collectDescendants(t.uid);
      }
    }
    collectDescendants(task.uid);
    return allTasks.filter((t) => !excludeUids.has(t.uid));
  }, [allTasks, task.uid]);

  const cal = resolveCalendar(task.calendarHref, calendars);
  const isOverdue = task.due && task.due < new Date() && task.status !== 'COMPLETED';

  const handlePomodoroSession = useCallback((sessionNumber: number, totalMinutes: number) => {
    cache.incrementStat('pomodoroSessions');
    const now = new Date();
    const timestamp = now.toISOString().replace('T', ' ').slice(0, 16);
    const sessionLog = `[Pomodoro] Session #${sessionNumber} completed (${totalMinutes}min) - ${timestamp}`;
    const totalLog = `[Pomodoro] Total: ${sessionNumber} session${sessionNumber !== 1 ? 's' : ''}, ${sessionNumber * totalMinutes}min`;

    const existingDesc = task.description || '';
    // Remove previous total line if present
    const lines = existingDesc.split('\n').filter((l) => !l.startsWith('[Pomodoro] Total:'));
    lines.push(sessionLog);
    lines.push(totalLog);
    const newDesc = lines.filter(Boolean).join('\n');

    onSave({
      ...task,
      description: newDesc,
      lastModified: now,
    });
  }, [task, onSave]);

  function handleSave() {
    if (!formSummary.trim()) return;
    const now = new Date();
    const priority = priorityFromLabel(formPriority);
    const percent = Math.max(0, Math.min(100, parseInt(formPercent, 10) || 0));
    const due = formDue ? new Date(formDue + 'T00:00:00') : undefined;
    const isCompleted = formStatus === 'COMPLETED';
    const categories = formCategories.split(',').map((s) => s.trim()).filter(Boolean);

    onSave({
      ...task,
      summary: formSummary,
      description: formDescription || undefined,
      status: formStatus,
      priority,
      percentComplete: isCompleted ? 100 : percent,
      due,
      completed: isCompleted ? (task.completed || now) : undefined,
      lastModified: now,
      parentUid: formParentUid || undefined,
      location: formLocation || undefined,
      categories: categories.length > 0 ? categories : undefined,
    });
    setEditing(false);
  }

  function handleToggleEdit() {
    if (editing) {
      // Reset form
      setFormSummary(task.summary);
      setFormDescription(task.description || '');
      setFormStatus(task.status);
      setFormPriority(getPriorityLabel(task.priority));
      setFormDue(task.due ? task.due.toISOString().split('T')[0] : '');
      setFormPercent(String(task.percentComplete));
      setFormParentUid(task.parentUid || '');
      setFormLocation(task.location || '');
      setFormCategories((task.categories || []).join(', '));
    }
    setEditing(!editing);
  }

  function handleSubtaskToggle(subtask: CalDavTask) {
    const now = new Date();
    const isCompleting = subtask.status !== 'COMPLETED';
    onSave({
      ...subtask,
      status: isCompleting ? 'COMPLETED' : 'NEEDS-ACTION',
      percentComplete: isCompleting ? 100 : 0,
      completed: isCompleting ? now : undefined,
      lastModified: now,
    });
  }

  return (
    <PropertyPageModal visible>
      <PropertyPageHeader
        title="Task"
        onClose={onClose}
        canEdit={canEdit}
        isEditing={editing}
        onToggleEdit={handleToggleEdit}
      />
      <ScrollView style={styles.body} contentContainerStyle={styles.bodyContent} keyboardShouldPersistTaps="handled">
        {editing ? (
          <View style={styles.formSection}>
            <Text style={[styles.formLabel, { color: c.textSecondary }]}>Summary</Text>
            <TextInput
              style={[styles.input, { borderColor: c.border, backgroundColor: c.inputBackground, color: c.text }]}
              value={formSummary}
              onChangeText={setFormSummary}
              placeholder="Task summary"
              placeholderTextColor={c.textTertiary}
              autoFocus
            />

            <Text style={[styles.formLabel, { color: c.textSecondary }]}>Description</Text>
            <TextInput
              style={[styles.input, styles.multilineInput, { borderColor: c.border, backgroundColor: c.inputBackground, color: c.text }]}
              value={formDescription}
              onChangeText={setFormDescription}
              placeholder="Description (optional)"
              placeholderTextColor={c.textTertiary}
              multiline
              textAlignVertical="top"
            />

            <Text style={[styles.formLabel, { color: c.textSecondary }]}>Status</Text>
            <View style={styles.segmentedRow}>
              {STATUS_OPTIONS.map((opt) => (
                <TouchableOpacity
                  key={opt.value}
                  style={[styles.segmentButton, { borderColor: c.border }, formStatus === opt.value && { backgroundColor: c.primary, borderColor: c.primary }]}
                  onPress={() => setFormStatus(opt.value)}
                >
                  <Text style={[styles.segmentText, { color: c.text }, formStatus === opt.value && { color: '#fff' }]}>{opt.label}</Text>
                </TouchableOpacity>
              ))}
            </View>

            <Text style={[styles.formLabel, { color: c.textSecondary }]}>Priority</Text>
            <View style={styles.segmentedRow}>
              {PRIORITY_OPTIONS.map((opt) => (
                <TouchableOpacity
                  key={opt}
                  style={[styles.segmentButton, { borderColor: c.border }, formPriority === opt && { backgroundColor: c.primary, borderColor: c.primary }]}
                  onPress={() => setFormPriority(opt)}
                >
                  <Text style={[styles.segmentText, { color: c.text }, formPriority === opt && { color: '#fff' }]}>{opt}</Text>
                </TouchableOpacity>
              ))}
            </View>

            <View style={styles.inlineRow}>
              <View style={styles.inlineField}>
                <Text style={[styles.formLabel, { color: c.textSecondary }]}>Due Date</Text>
                <TextInput
                  style={[styles.input, { borderColor: c.border, backgroundColor: c.inputBackground, color: c.text }]}
                  value={formDue}
                  onChangeText={setFormDue}
                  placeholder="YYYY-MM-DD"
                  placeholderTextColor={c.textTertiary}
                />
              </View>
              <View style={styles.inlineField}>
                <Text style={[styles.formLabel, { color: c.textSecondary }]}>% Complete</Text>
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

            <Text style={[styles.formLabel, { color: c.textSecondary }]}>Location</Text>
            <TextInput
              style={[styles.input, { borderColor: c.border, backgroundColor: c.inputBackground, color: c.text }]}
              value={formLocation}
              onChangeText={setFormLocation}
              placeholder="Location (optional)"
              placeholderTextColor={c.textTertiary}
            />

            <Text style={[styles.formLabel, { color: c.textSecondary }]}>Categories (comma-separated)</Text>
            <TextInput
              style={[styles.input, { borderColor: c.border, backgroundColor: c.inputBackground, color: c.text }]}
              value={formCategories}
              onChangeText={setFormCategories}
              placeholder="work, personal"
              placeholderTextColor={c.textTertiary}
            />
            {/* Quick-add tag chips */}
            {(() => {
              const existingTags = new Set(
                formCategories.split(',').map((s) => s.trim().toLowerCase()).filter(Boolean)
              );
              const appendTag = (tag: string) => {
                const current = formCategories.trim();
                setFormCategories(current ? `${current}, ${tag}` : tag);
              };
              const allQuickTags = [
                ...kanbanColumns,
                ...QUADRANT_QUICK_TAGS,
                ...TIME_QUICK_TAGS,
              ].filter((tag, idx, arr) => arr.indexOf(tag) === idx); // dedupe
              return (
                <View style={styles.quickTagsRow}>
                  {allQuickTags.map((tag) => {
                    const active = existingTags.has(tag.toLowerCase());
                    return (
                      <TouchableOpacity
                        key={tag}
                        style={[
                          styles.quickTagChip,
                          { borderColor: c.primary, backgroundColor: active ? c.primary : 'transparent' },
                        ]}
                        onPress={() => !active && appendTag(tag)}
                      >
                        <Text style={[styles.quickTagChipText, { color: active ? '#fff' : c.primary }]}>
                          #{tag}
                        </Text>
                      </TouchableOpacity>
                    );
                  })}
                </View>
              );
            })()}

            <Text style={[styles.formLabel, { color: c.textSecondary, marginTop: 8 }]}>Parent Task</Text>
            <View style={styles.parentPicker}>
              <TouchableOpacity
                style={[styles.parentOption, { borderColor: c.border }, !formParentUid && { backgroundColor: c.primary, borderColor: c.primary }]}
                onPress={() => setFormParentUid('')}
              >
                <Text style={[styles.parentOptionText, { color: c.text }, !formParentUid && { color: '#fff' }]}>None</Text>
              </TouchableOpacity>
              {availableParents.slice(0, 5).map((t) => (
                <TouchableOpacity
                  key={t.uid}
                  style={[styles.parentOption, { borderColor: c.border }, formParentUid === t.uid && { backgroundColor: c.primary, borderColor: c.primary }]}
                  onPress={() => setFormParentUid(t.uid)}
                >
                  <Text style={[styles.parentOptionText, { color: c.text }, formParentUid === t.uid && { color: '#fff' }]} numberOfLines={1}>{t.summary}</Text>
                </TouchableOpacity>
              ))}
            </View>

            <TouchableOpacity style={[styles.saveButton, { backgroundColor: c.primary }]} onPress={handleSave}>
              <Text style={styles.saveButtonText}>Save</Text>
            </TouchableOpacity>
          </View>
        ) : (
          <>
            <Text style={[styles.summary, { color: c.text }]}>{task.summary}</Text>

            {/* Status badge + calendar + pomodoro */}
            <View style={styles.badgeRow}>
              <View style={[styles.statusBadge, { backgroundColor: getStatusColor(task.status) }]}>
                <Text style={styles.statusBadgeText}>{getStatusLabel(task.status)}</Text>
              </View>
              {cal && (
                <View style={styles.calChip}>
                  {cal.color && <View style={[styles.calDot, { backgroundColor: cal.color }]} />}
                  <Text style={[styles.calName, { color: c.textSecondary }]}>{cal.displayName}</Text>
                </View>
              )}
              <TouchableOpacity onPress={() => pomodoro.start(task.summary, handlePomodoroSession)} style={styles.pomodoroButton}>
                <AppIcon name={Icons.play} size={22} color={c.primary} />
              </TouchableOpacity>
            </View>

            {/* Priority */}
            {task.priority > 0 && (
              <View style={styles.fieldRowInline}>
                <Text style={[styles.inlineLabel, { color: c.textSecondary }]}>Priority</Text>
                <View style={styles.priorityChip}>
                  <View style={[styles.priorityDot, { backgroundColor: getPriorityColor(task.priority) || c.textTertiary }]} />
                  <Text style={[styles.inlineValue, { color: c.text }]}>{getPriorityLabel(task.priority)}</Text>
                </View>
              </View>
            )}

            {/* Due date */}
            {task.due && (
              <ReadField
                label="Due Date"
                value={task.due.toLocaleString(undefined, { weekday: 'short', month: 'short', day: 'numeric', year: 'numeric', hour: '2-digit', minute: '2-digit' })}
                valueColor={isOverdue ? '#F44336' : undefined}
              />
            )}
            {task.dtstart && (
              <ReadField label="Start Date" value={task.dtstart.toLocaleString()} />
            )}
            {task.completed && (
              <ReadField label="Completed" value={task.completed.toLocaleString()} />
            )}

            {/* Progress */}
            {task.percentComplete > 0 && (
              <View style={styles.progressSection}>
                <Text style={[styles.inlineLabel, { color: c.textSecondary, paddingHorizontal: 20 }]}>Progress</Text>
                <View style={styles.progressRow}>
                  <View style={[styles.progressBar, { backgroundColor: c.border }]}>
                    <View style={[styles.progressFill, { width: `${task.percentComplete}%` }]} />
                  </View>
                  <Text style={[styles.progressText, { color: c.text }]}>{task.percentComplete}%</Text>
                </View>
              </View>
            )}

            {/* Categories */}
            {task.categories && task.categories.length > 0 && (
              <View style={styles.tagSection}>
                <Text style={[styles.inlineLabel, { color: c.textSecondary, paddingHorizontal: 20, marginBottom: 6 }]}>Categories</Text>
                <View style={styles.tagRow}>
                  {task.categories.map((cat) => (
                    <View key={cat} style={[styles.tagChip, { backgroundColor: '#E3F2FD' }]}>
                      <Text style={styles.tagChipText}>#{cat}</Text>
                    </View>
                  ))}
                </View>
              </View>
            )}

            {task.location && <ReadField label="Location" value={task.location} />}

            {/* Description */}
            {task.description ? (
              <>
                <SectionHeader title="Description / Notes" />
                <Text style={[styles.descText, { color: c.text }]}>{task.description}</Text>
              </>
            ) : null}

            {/* Subtasks */}
            {subtasks.length > 0 && (
              <>
                <SectionHeader title={`Subtasks (${subtasks.length})`} />
                {subtasks.map((st) => {
                  const stCompleted = st.status === 'COMPLETED';
                  const stPriColor = getPriorityColor(st.priority);
                  return (
                    <TouchableOpacity
                      key={st.uid}
                      style={[styles.subtaskRow, { borderBottomColor: c.borderLight }]}
                      onPress={() => handleSubtaskToggle(st)}
                    >
                      <AppIcon
                        name={stCompleted ? Icons.task : Icons.taskOutline}
                        size={20}
                        color={stCompleted ? '#4CAF50' : c.textTertiary}
                      />
                      <View style={styles.subtaskContent}>
                        <Text
                          style={[styles.subtaskTitle, { color: c.text }, stCompleted && styles.subtaskCompleted]}
                          numberOfLines={1}
                        >
                          {st.summary}
                        </Text>
                        <View style={styles.subtaskMeta}>
                          {st.due && (
                            <Text style={[styles.subtaskDue, { color: c.textTertiary }]}>
                              {st.due.toLocaleDateString(undefined, { month: 'short', day: 'numeric' })}
                            </Text>
                          )}
                          {stPriColor && <View style={[styles.priorityDot, { backgroundColor: stPriColor }]} />}
                        </View>
                      </View>
                    </TouchableOpacity>
                  );
                })}
              </>
            )}

            {/* Metadata */}
            <SectionHeader title="Metadata" />
            <ReadField label="Created" value={task.created.toLocaleString()} />
            <ReadField label="Modified" value={task.lastModified.toLocaleString()} />
            <ReadField label="UID" value={task.uid} valueColor={c.textTertiary} />

            <TouchableOpacity
              style={[styles.deleteButtonOuter, { borderColor: '#F44336' }]}
              onPress={() => onDelete(task.uid)}
            >
              <Text style={styles.deleteButtonText}>Delete Task</Text>
            </TouchableOpacity>
          </>
        )}
      </ScrollView>
    </PropertyPageModal>
  );
}

const styles = StyleSheet.create({
  body: { flex: 1 },
  bodyContent: { paddingBottom: 40 },
  summary: { fontSize: 22, fontWeight: '700', paddingHorizontal: 20, paddingTop: 20, paddingBottom: 8 },
  badgeRow: { flexDirection: 'row', alignItems: 'center', gap: 10, paddingHorizontal: 20, paddingBottom: 12, flexWrap: 'wrap' },
  pomodoroButton: { marginLeft: 'auto', padding: 4 },
  statusBadge: { paddingHorizontal: 10, paddingVertical: 4, borderRadius: 12 },
  statusBadgeText: { color: '#fff', fontSize: 13, fontWeight: '600' },
  calChip: { flexDirection: 'row', alignItems: 'center', gap: 6 },
  calDot: { width: 12, height: 12, borderRadius: 6 },
  calName: { fontSize: 14 },
  fieldRowInline: { flexDirection: 'row', alignItems: 'center', paddingHorizontal: 20, paddingVertical: 6 },
  inlineLabel: { fontSize: 12, fontWeight: '600', textTransform: 'uppercase', width: 100 },
  inlineValue: { fontSize: 15 },
  priorityChip: { flexDirection: 'row', alignItems: 'center', gap: 6 },
  priorityDot: { width: 10, height: 10, borderRadius: 5 },
  progressSection: { paddingVertical: 8 },
  progressRow: { flexDirection: 'row', alignItems: 'center', gap: 10, paddingHorizontal: 20, marginTop: 4 },
  progressBar: { flex: 1, height: 8, borderRadius: 4, overflow: 'hidden' },
  progressFill: { height: 8, backgroundColor: '#4CAF50', borderRadius: 4 },
  progressText: { fontSize: 14, fontWeight: '600', width: 40, textAlign: 'right' },
  tagSection: { paddingVertical: 8 },
  tagRow: { flexDirection: 'row', flexWrap: 'wrap', gap: 6, paddingHorizontal: 20 },
  tagChip: { paddingHorizontal: 10, paddingVertical: 4, borderRadius: 12 },
  tagChipText: { fontSize: 13, color: '#1565C0', fontWeight: '500' },
  descText: { fontSize: 15, lineHeight: 22, paddingHorizontal: 20 },
  subtaskRow: { flexDirection: 'row', alignItems: 'center', gap: 10, paddingHorizontal: 20, paddingVertical: 10, borderBottomWidth: 1 },
  subtaskContent: { flex: 1 },
  subtaskTitle: { fontSize: 15 },
  subtaskCompleted: { textDecorationLine: 'line-through', opacity: 0.6 },
  subtaskMeta: { flexDirection: 'row', alignItems: 'center', gap: 8, marginTop: 2 },
  subtaskDue: { fontSize: 12 },
  formSection: { padding: 20 },
  formLabel: { fontSize: 13, fontWeight: '600', marginBottom: 6 },
  input: { borderWidth: 1, borderRadius: 8, padding: 12, fontSize: 15, marginBottom: 16 },
  multilineInput: { minHeight: 80 },
  segmentedRow: { flexDirection: 'row', flexWrap: 'wrap', gap: 6, marginBottom: 16 },
  segmentButton: { borderWidth: 1, borderRadius: 6, paddingHorizontal: 10, paddingVertical: 6 },
  segmentText: { fontSize: 12, fontWeight: '500' },
  inlineRow: { flexDirection: 'row', gap: 12 },
  inlineField: { flex: 1 },
  parentPicker: { flexDirection: 'row', flexWrap: 'wrap', gap: 6, marginBottom: 16 },
  parentOption: { borderWidth: 1, borderRadius: 6, paddingHorizontal: 10, paddingVertical: 6, maxWidth: 160 },
  parentOptionText: { fontSize: 12 },
  quickTagsRow: { flexDirection: 'row', flexWrap: 'wrap', gap: 6, marginBottom: 8 },
  quickTagChip: { borderWidth: 1, borderRadius: 14, paddingHorizontal: 10, paddingVertical: 4 },
  quickTagChipText: { fontSize: 12, fontWeight: '500' },
  saveButton: { padding: 14, borderRadius: 8, alignItems: 'center', marginTop: 8 },
  saveButtonText: { color: '#fff', fontSize: 16, fontWeight: '600' },
  deleteButtonOuter: { marginHorizontal: 20, marginTop: 24, padding: 14, borderRadius: 8, borderWidth: 1, alignItems: 'center' },
  deleteButtonText: { color: '#F44336', fontSize: 15, fontWeight: '600' },
});
