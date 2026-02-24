import React, { useState } from 'react';
import {
  View,
  Text,
  StyleSheet,
  ScrollView,
  TextInput,
  TouchableOpacity,
} from 'react-native';
import { useTheme } from '../hooks/useTheme';
import { Note } from '../types';
import {
  PropertyPageModal,
  PropertyPageHeader,
  ReadField,
  SectionHeader,
} from './PropertyPageShared';

interface Props {
  note: Note;
  onClose: () => void;
  onSave: (updated: Note) => void;
  onDelete: (uid: string) => void;
  canEdit: boolean;
}

export default function NotePropertyPage({ note, onClose, onSave, onDelete, canEdit }: Props) {
  const theme = useTheme();
  const c = theme.colors;
  const [editing, setEditing] = useState(false);

  const [formTitle, setFormTitle] = useState(note.title);
  const [formContent, setFormContent] = useState(note.content);
  const [formTags, setFormTags] = useState((note.tags || []).join(', '));

  function handleSave() {
    if (!formTitle.trim()) return;
    const tags = formTags
      .split(',')
      .map((t) => t.trim())
      .filter(Boolean);
    onSave({
      ...note,
      title: formTitle,
      content: formContent,
      tags: tags.length > 0 ? tags : undefined,
      updatedAt: new Date(),
    });
    setEditing(false);
  }

  function handleToggleEdit() {
    if (editing) {
      // Cancel edit — reset form
      setFormTitle(note.title);
      setFormContent(note.content);
      setFormTags((note.tags || []).join(', '));
    }
    setEditing(!editing);
  }

  return (
    <PropertyPageModal visible>
      <PropertyPageHeader
        title="Note"
        onClose={onClose}
        canEdit={canEdit}
        isEditing={editing}
        onToggleEdit={handleToggleEdit}
      />
      <ScrollView style={styles.body} contentContainerStyle={styles.bodyContent} keyboardShouldPersistTaps="handled">
        {editing ? (
          <>
            <View style={styles.formSection}>
              <Text style={[styles.formLabel, { color: c.textSecondary }]}>Title</Text>
              <TextInput
                style={[styles.input, { borderColor: c.border, backgroundColor: c.inputBackground, color: c.text }]}
                value={formTitle}
                onChangeText={setFormTitle}
                placeholder="Title"
                placeholderTextColor={c.textTertiary}
                autoFocus
              />

              <Text style={[styles.formLabel, { color: c.textSecondary }]}>Content</Text>
              <TextInput
                style={[styles.input, styles.contentInput, { borderColor: c.border, backgroundColor: c.inputBackground, color: c.text }]}
                value={formContent}
                onChangeText={setFormContent}
                placeholder="Write your note..."
                placeholderTextColor={c.textTertiary}
                multiline
                textAlignVertical="top"
              />

              <Text style={[styles.formLabel, { color: c.textSecondary }]}>Tags (comma-separated)</Text>
              <TextInput
                style={[styles.input, { borderColor: c.border, backgroundColor: c.inputBackground, color: c.text }]}
                value={formTags}
                onChangeText={setFormTags}
                placeholder="tag1, tag2"
                placeholderTextColor={c.textTertiary}
              />

              <TouchableOpacity style={[styles.saveButton, { backgroundColor: c.primary }]} onPress={handleSave}>
                <Text style={styles.saveButtonText}>Save</Text>
              </TouchableOpacity>
            </View>
          </>
        ) : (
          <>
            <Text style={[styles.title, { color: c.text }]}>{note.title}</Text>

            {note.tags && note.tags.length > 0 && (
              <View style={styles.tagRow}>
                {note.tags.map((tag) => (
                  <View key={tag} style={[styles.tagChip, { backgroundColor: c.filterChip }]}>
                    <Text style={[styles.tagChipText, { color: c.textSecondary }]}>#{tag}</Text>
                  </View>
                ))}
              </View>
            )}

            <SectionHeader title="Content" />
            <Text style={[styles.contentText, { color: c.text }]}>
              {note.content || '(empty)'}
            </Text>

            <SectionHeader title="Metadata" />
            <ReadField label="Created" value={note.createdAt.toLocaleString()} />
            <ReadField label="Updated" value={note.updatedAt.toLocaleString()} />
            <ReadField label="UID" value={note.uid} valueColor={c.textTertiary} />

            <TouchableOpacity
              style={[styles.deleteButton, { borderColor: '#F44336' }]}
              onPress={() => onDelete(note.uid)}
            >
              <Text style={styles.deleteButtonText}>Delete Note</Text>
            </TouchableOpacity>
          </>
        )}
      </ScrollView>
    </PropertyPageModal>
  );
}

const styles = StyleSheet.create({
  body: {
    flex: 1,
  },
  bodyContent: {
    paddingBottom: 40,
  },
  title: {
    fontSize: 22,
    fontWeight: '700',
    paddingHorizontal: 20,
    paddingTop: 20,
    paddingBottom: 8,
  },
  tagRow: {
    flexDirection: 'row',
    flexWrap: 'wrap',
    gap: 6,
    paddingHorizontal: 20,
    paddingBottom: 8,
  },
  tagChip: {
    paddingHorizontal: 10,
    paddingVertical: 4,
    borderRadius: 12,
  },
  tagChipText: {
    fontSize: 13,
    fontWeight: '500',
  },
  contentText: {
    fontSize: 15,
    lineHeight: 22,
    paddingHorizontal: 20,
  },
  formSection: {
    padding: 20,
  },
  formLabel: {
    fontSize: 13,
    fontWeight: '600',
    marginBottom: 6,
  },
  input: {
    borderWidth: 1,
    borderRadius: 8,
    padding: 12,
    fontSize: 15,
    marginBottom: 16,
  },
  contentInput: {
    minHeight: 300,
  },
  saveButton: {
    padding: 14,
    borderRadius: 8,
    alignItems: 'center',
    marginTop: 8,
  },
  saveButtonText: {
    color: '#fff',
    fontSize: 16,
    fontWeight: '600',
  },
  deleteButton: {
    marginHorizontal: 20,
    marginTop: 24,
    padding: 14,
    borderRadius: 8,
    borderWidth: 1,
    alignItems: 'center',
  },
  deleteButtonText: {
    color: '#F44336',
    fontSize: 15,
    fontWeight: '600',
  },
});
