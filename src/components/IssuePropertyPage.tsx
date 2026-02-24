import React, { useState, useEffect } from 'react';
import {
  View,
  Text,
  StyleSheet,
  ScrollView,
  TextInput,
  TouchableOpacity,
  ActivityIndicator,
  Linking,
  Image,
} from 'react-native';
import { useTheme } from '../hooks/useTheme';
import { GiteaIssue, GiteaComment } from '../types';
import * as gitea from '../services/gitea';
import AppIcon, { Icons } from './Icon';
import { usePomodoro } from '../store/PomodoroContext';
import {
  PropertyPageModal,
  PropertyPageHeader,
  ReadField,
  SectionHeader,
} from './PropertyPageShared';

interface Props {
  issue: GiteaIssue;
  onClose: () => void;
  onSave: (updated: GiteaIssue) => void;
  onStateToggle: (issue: GiteaIssue) => void;
  canEdit: boolean;
}

export default function IssuePropertyPage({ issue, onClose, onSave, onStateToggle, canEdit }: Props) {
  const theme = useTheme();
  const c = theme.colors;
  const pomodoro = usePomodoro();
  const [editing, setEditing] = useState(false);

  // Form state
  const [formTitle, setFormTitle] = useState(issue.title);
  const [formBody, setFormBody] = useState(issue.body);
  const [formState, setFormState] = useState<'open' | 'closed'>(issue.state);

  // Comments
  const [comments, setComments] = useState<GiteaComment[]>([]);
  const [loadingComments, setLoadingComments] = useState(true);
  const [newComment, setNewComment] = useState('');
  const [submitting, setSubmitting] = useState(false);

  const [owner, repo] = issue.repository.split('/');

  useEffect(() => {
    loadComments();
  }, [issue.number]);

  async function loadComments() {
    setLoadingComments(true);
    const data = await gitea.fetchComments(owner, repo, issue.number);
    setComments(data);
    setLoadingComments(false);
  }

  async function handleAddComment() {
    if (!newComment.trim() || submitting) return;
    setSubmitting(true);
    const created = await gitea.addComment(owner, repo, issue.number, newComment);
    if (created) {
      setComments((prev) => [...prev, created]);
      setNewComment('');
    }
    setSubmitting(false);
  }

  async function handleSave() {
    if (!formTitle.trim()) return;
    const success = await gitea.updateIssue(owner, repo, issue.number, {
      title: formTitle,
      body: formBody,
      state: formState,
    });
    if (success) {
      onSave({ ...issue, title: formTitle, body: formBody, state: formState, updatedAt: new Date() });
    }
    setEditing(false);
  }

  function handleToggleEdit() {
    if (editing) {
      setFormTitle(issue.title);
      setFormBody(issue.body);
      setFormState(issue.state);
    }
    setEditing(!editing);
  }

  return (
    <PropertyPageModal visible>
      <PropertyPageHeader
        title="Issue"
        onClose={onClose}
        canEdit={canEdit}
        isEditing={editing}
        onToggleEdit={handleToggleEdit}
      />
      <ScrollView style={styles.body} contentContainerStyle={styles.bodyContent} keyboardShouldPersistTaps="handled">
        {editing ? (
          <View style={styles.formSection}>
            <Text style={[styles.formLabel, { color: c.textSecondary }]}>Title</Text>
            <TextInput
              style={[styles.input, { borderColor: c.border, backgroundColor: c.inputBackground, color: c.text }]}
              value={formTitle}
              onChangeText={setFormTitle}
              placeholder="Issue title"
              placeholderTextColor={c.textTertiary}
              autoFocus
            />

            <Text style={[styles.formLabel, { color: c.textSecondary }]}>Body</Text>
            <TextInput
              style={[styles.input, styles.bodyInput, { borderColor: c.border, backgroundColor: c.inputBackground, color: c.text }]}
              value={formBody}
              onChangeText={setFormBody}
              placeholder="Issue body"
              placeholderTextColor={c.textTertiary}
              multiline
              textAlignVertical="top"
            />

            <Text style={[styles.formLabel, { color: c.textSecondary }]}>State</Text>
            <View style={styles.segmentedRow}>
              {(['open', 'closed'] as const).map((s) => (
                <TouchableOpacity
                  key={s}
                  style={[styles.segmentButton, { borderColor: c.border }, formState === s && { backgroundColor: c.primary, borderColor: c.primary }]}
                  onPress={() => setFormState(s)}
                >
                  <Text style={[styles.segmentText, { color: c.text }, formState === s && { color: '#fff' }]}>
                    {s.charAt(0).toUpperCase() + s.slice(1)}
                  </Text>
                </TouchableOpacity>
              ))}
            </View>

            <TouchableOpacity style={[styles.saveButton, { backgroundColor: c.primary }]} onPress={handleSave}>
              <Text style={styles.saveButtonText}>Save</Text>
            </TouchableOpacity>
          </View>
        ) : (
          <>
            <Text style={[styles.title, { color: c.text }]}>{issue.title}</Text>

            {/* State badge + repo + pomodoro */}
            <View style={styles.badgeRow}>
              <View style={[styles.stateBadge, issue.state === 'open' ? styles.stateOpen : styles.stateClosed]}>
                <Text style={styles.stateBadgeText}>{issue.state}</Text>
              </View>
              <Text style={[styles.repoName, { color: c.textSecondary }]}>
                {issue.repository} #{issue.number}
              </Text>
              <TouchableOpacity onPress={() => pomodoro.start(issue.title)} style={styles.pomodoroButton}>
                <AppIcon name={Icons.play} size={22} color={c.primary} />
              </TouchableOpacity>
            </View>

            {/* Labels */}
            {issue.labels.length > 0 && (
              <View style={styles.labelRow}>
                {issue.labels.map((label) => (
                  <View key={label.id} style={[styles.labelChip, { backgroundColor: `#${label.color}` }]}>
                    <Text style={styles.labelChipText}>{label.name}</Text>
                  </View>
                ))}
              </View>
            )}

            {/* Assignees */}
            {issue.assignees.length > 0 && (
              <View style={styles.assigneeRow}>
                <Text style={[styles.assigneeLabel, { color: c.textSecondary }]}>Assignees:</Text>
                {issue.assignees.map((a) => (
                  <View key={a.id} style={styles.assigneeChip}>
                    <Image source={{ uri: a.avatarUrl }} style={styles.assigneeAvatar} />
                    <Text style={[styles.assigneeName, { color: c.text }]}>{a.login}</Text>
                  </View>
                ))}
              </View>
            )}

            <ReadField label="Created" value={issue.createdAt.toLocaleString()} />
            <ReadField label="Updated" value={issue.updatedAt.toLocaleString()} />

            {/* Body */}
            {issue.body ? (
              <>
                <SectionHeader title="Description" />
                <Text style={[styles.bodyText, { color: c.text }]}>{issue.body}</Text>
              </>
            ) : null}

            {/* Open/Close toggle */}
            <TouchableOpacity
              style={[styles.toggleButton, { backgroundColor: issue.state === 'open' ? '#F44336' : '#4CAF50' }]}
              onPress={() => onStateToggle(issue)}
            >
              <Text style={styles.toggleButtonText}>
                {issue.state === 'open' ? 'Close Issue' : 'Reopen Issue'}
              </Text>
            </TouchableOpacity>

            {/* Comments */}
            <SectionHeader title={`Comments${!loadingComments ? ` (${comments.length})` : ''}`} />
            {loadingComments ? (
              <ActivityIndicator size="small" color={c.primary} style={{ marginTop: 12 }} />
            ) : (
              comments.map((cm) => (
                <View key={cm.id} style={[styles.commentCard, { backgroundColor: c.background, borderColor: c.border }]}>
                  <View style={styles.commentHeader}>
                    <Image source={{ uri: cm.user.avatarUrl }} style={styles.commentAvatar} />
                    <Text style={[styles.commentUser, { color: c.text }]}>{cm.user.login}</Text>
                    <Text style={[styles.commentDate, { color: c.textTertiary }]}>
                      {cm.createdAt.toLocaleDateString()}
                    </Text>
                  </View>
                  <Text style={[styles.commentBody, { color: c.text }]}>{cm.body}</Text>
                </View>
              ))
            )}

            {/* Add comment */}
            <View style={styles.addCommentSection}>
              <TextInput
                style={[styles.input, styles.commentInput, { borderColor: c.border, backgroundColor: c.inputBackground, color: c.text }]}
                value={newComment}
                onChangeText={setNewComment}
                placeholder="Write a comment..."
                placeholderTextColor={c.textTertiary}
                multiline
                textAlignVertical="top"
              />
              <TouchableOpacity
                style={[styles.submitCommentButton, { backgroundColor: c.primary, opacity: newComment.trim() ? 1 : 0.5 }]}
                onPress={handleAddComment}
                disabled={!newComment.trim() || submitting}
              >
                {submitting ? (
                  <ActivityIndicator size="small" color="#fff" />
                ) : (
                  <Text style={styles.submitCommentText}>Submit</Text>
                )}
              </TouchableOpacity>
            </View>

            {/* Open in browser */}
            <TouchableOpacity
              style={styles.browserLink}
              onPress={() => Linking.openURL(issue.htmlUrl)}
            >
              <AppIcon name={Icons.link} size={16} color={c.primary} />
              <Text style={[styles.browserLinkText, { color: c.primary }]}>Open in Browser</Text>
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
  title: { fontSize: 22, fontWeight: '700', paddingHorizontal: 20, paddingTop: 20, paddingBottom: 8 },
  badgeRow: { flexDirection: 'row', alignItems: 'center', gap: 10, paddingHorizontal: 20, paddingBottom: 12, flexWrap: 'wrap' },
  pomodoroButton: { marginLeft: 'auto', padding: 4 },
  stateBadge: { paddingHorizontal: 10, paddingVertical: 4, borderRadius: 12 },
  stateOpen: { backgroundColor: '#4CAF50' },
  stateClosed: { backgroundColor: '#9E9E9E' },
  stateBadgeText: { color: '#fff', fontSize: 13, fontWeight: '600' },
  repoName: { fontSize: 14 },
  labelRow: { flexDirection: 'row', flexWrap: 'wrap', gap: 6, paddingHorizontal: 20, paddingBottom: 10 },
  labelChip: { paddingHorizontal: 10, paddingVertical: 4, borderRadius: 12 },
  labelChipText: { color: '#fff', fontSize: 12, fontWeight: '600' },
  assigneeRow: { flexDirection: 'row', alignItems: 'center', flexWrap: 'wrap', gap: 8, paddingHorizontal: 20, paddingBottom: 10 },
  assigneeLabel: { fontSize: 12, fontWeight: '600', textTransform: 'uppercase' },
  assigneeChip: { flexDirection: 'row', alignItems: 'center', gap: 6 },
  assigneeAvatar: { width: 22, height: 22, borderRadius: 11 },
  assigneeName: { fontSize: 14 },
  bodyText: { fontSize: 15, lineHeight: 22, paddingHorizontal: 20 },
  toggleButton: { marginHorizontal: 20, marginTop: 16, padding: 12, borderRadius: 8, alignItems: 'center' },
  toggleButtonText: { color: '#fff', fontSize: 15, fontWeight: '600' },
  commentCard: { marginHorizontal: 20, marginTop: 10, padding: 12, borderRadius: 8, borderWidth: 1 },
  commentHeader: { flexDirection: 'row', alignItems: 'center', gap: 8, marginBottom: 8 },
  commentAvatar: { width: 24, height: 24, borderRadius: 12 },
  commentUser: { fontSize: 14, fontWeight: '600', flex: 1 },
  commentDate: { fontSize: 12 },
  commentBody: { fontSize: 14, lineHeight: 20 },
  addCommentSection: { paddingHorizontal: 20, paddingTop: 16 },
  commentInput: { minHeight: 80 },
  submitCommentButton: { padding: 12, borderRadius: 8, alignItems: 'center', marginTop: 10 },
  submitCommentText: { color: '#fff', fontSize: 15, fontWeight: '600' },
  browserLink: { flexDirection: 'row', alignItems: 'center', gap: 6, paddingHorizontal: 20, paddingTop: 20, paddingBottom: 10 },
  browserLinkText: { fontSize: 14, fontWeight: '500' },
  formSection: { padding: 20 },
  formLabel: { fontSize: 13, fontWeight: '600', marginBottom: 6 },
  input: { borderWidth: 1, borderRadius: 8, padding: 12, fontSize: 15, marginBottom: 16 },
  bodyInput: { minHeight: 150 },
  segmentedRow: { flexDirection: 'row', gap: 6, marginBottom: 16 },
  segmentButton: { borderWidth: 1, borderRadius: 6, paddingHorizontal: 14, paddingVertical: 8 },
  segmentText: { fontSize: 13, fontWeight: '500' },
  saveButton: { padding: 14, borderRadius: 8, alignItems: 'center', marginTop: 8 },
  saveButtonText: { color: '#fff', fontSize: 16, fontWeight: '600' },
});
