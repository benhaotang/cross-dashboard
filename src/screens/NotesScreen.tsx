import React, { useState } from 'react';
import {
  View,
  Text,
  StyleSheet,
  FlatList,
  TouchableOpacity,
  TextInput,
  Modal,
} from 'react-native';
import { useApp } from '../store/AppContext';
import { Note } from '../types';
import AppIcon, { Icons } from '../components/Icon';

export default function NotesScreen() {
  const { state, setNotes } = useApp();
  const [modalVisible, setModalVisible] = useState(false);
  const [newTitle, setNewTitle] = useState('');
  const [newContent, setNewContent] = useState('');
  const [selectedNote, setSelectedNote] = useState<Note | null>(null);

  function openNewNote() {
    setSelectedNote(null);
    setNewTitle('');
    setNewContent('');
    setModalVisible(true);
  }

  function openEditNote(note: Note) {
    setSelectedNote(note);
    setNewTitle(note.title);
    setNewContent(note.content);
    setModalVisible(true);
  }

  function saveNote() {
    if (!newTitle.trim()) return;

    const now = new Date();

    if (selectedNote) {
      const updated = state.notes.map((n) =>
        n.uid === selectedNote.uid ? { ...n, title: newTitle, content: newContent, updatedAt: now } : n
      );
      setNotes(updated);
    } else {
      const newNote: Note = {
        uid: `note-${Date.now()}`,
        title: newTitle,
        content: newContent,
        createdAt: now,
        updatedAt: now,
      };
      setNotes([newNote, ...state.notes]);
    }

    setModalVisible(false);
    setNewTitle('');
    setNewContent('');
    setSelectedNote(null);
  }

  function deleteNote(uid: string) {
    setNotes(state.notes.filter((n) => n.uid !== uid));
  }

  function renderNote({ item }: { item: Note }) {
    return (
      <TouchableOpacity style={styles.noteCard} onPress={() => openEditNote(item)}>
        <View style={styles.noteHeader}>
          <Text style={styles.noteTitle} numberOfLines={1}>
            {item.title}
          </Text>
          <TouchableOpacity onPress={() => deleteNote(item.uid)} style={styles.deleteButton}>
            <AppIcon name={Icons.delete} size={18} color="#F44336" />
          </TouchableOpacity>
        </View>
        <Text style={styles.noteContent} numberOfLines={3}>
          {item.content}
        </Text>
        <Text style={styles.noteDate}>
          {item.updatedAt.toLocaleDateString()} {item.updatedAt.toLocaleTimeString()}
        </Text>
      </TouchableOpacity>
    );
  }

  return (
    <View style={styles.container}>
      <View style={styles.header}>
        <Text style={styles.headerTitle}>Notes</Text>
        <TouchableOpacity style={styles.addButton} onPress={openNewNote}>
          <AppIcon name={Icons.add} size={18} color="#fff" />
          <Text style={styles.addButtonText}>New</Text>
        </TouchableOpacity>
      </View>

      <FlatList
        data={state.notes}
        renderItem={renderNote}
        keyExtractor={(item) => item.uid}
        contentContainerStyle={styles.listContent}
        ListEmptyComponent={
          <View style={styles.emptyContainer}>
            <Text style={styles.emptyText}>No notes yet</Text>
            <Text style={styles.hintText}>Tap "+ New" to create your first note</Text>
          </View>
        }
      />

      <Modal visible={modalVisible} animationType="slide" transparent>
        <View style={styles.modalOverlay}>
          <View style={styles.modalContent}>
            <View style={styles.modalHeader}>
              <Text style={styles.modalTitle}>{selectedNote ? 'Edit Note' : 'New Note'}</Text>
              <TouchableOpacity onPress={() => setModalVisible(false)}>
                <Text style={styles.cancelText}>Cancel</Text>
              </TouchableOpacity>
            </View>

            <TextInput
              style={styles.titleInput}
              value={newTitle}
              onChangeText={setNewTitle}
              placeholder="Title"
              autoFocus
            />

            <TextInput
              style={styles.contentInput}
              value={newContent}
              onChangeText={setNewContent}
              placeholder="Write your note..."
              multiline
              textAlignVertical="top"
            />

            <TouchableOpacity style={styles.saveButton} onPress={saveNote}>
              <Text style={styles.saveButtonText}>Save</Text>
            </TouchableOpacity>
          </View>
        </View>
      </Modal>
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
  headerTitle: {
    fontSize: 20,
    fontWeight: '600',
  },
  addButton: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 6,
    backgroundColor: '#007AFF',
    paddingHorizontal: 16,
    paddingVertical: 8,
    borderRadius: 8,
  },
  addButtonText: {
    color: '#fff',
    fontWeight: '600',
  },
  listContent: {
    padding: 16,
    flexGrow: 1,
  },
  noteCard: {
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
  noteHeader: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    marginBottom: 8,
  },
  noteTitle: {
    fontSize: 16,
    fontWeight: '600',
    flex: 1,
    marginRight: 8,
  },
  deleteButton: {
    padding: 4,
  },
  deleteText: {
    color: '#F44336',
    fontSize: 12,
  },
  noteContent: {
    fontSize: 14,
    color: '#666',
    lineHeight: 20,
  },
  noteDate: {
    fontSize: 12,
    color: '#999',
    marginTop: 8,
  },
  emptyContainer: {
    flex: 1,
    justifyContent: 'center',
    alignItems: 'center',
    paddingTop: 100,
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
  modalOverlay: {
    flex: 1,
    backgroundColor: 'rgba(0,0,0,0.5)',
    justifyContent: 'flex-end',
  },
  modalContent: {
    backgroundColor: '#fff',
    borderTopLeftRadius: 16,
    borderTopRightRadius: 16,
    padding: 20,
    minHeight: '60%',
  },
  modalHeader: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    marginBottom: 20,
  },
  modalTitle: {
    fontSize: 18,
    fontWeight: '600',
  },
  cancelText: {
    color: '#007AFF',
    fontSize: 16,
  },
  titleInput: {
    borderWidth: 1,
    borderColor: '#ddd',
    borderRadius: 8,
    padding: 12,
    fontSize: 16,
    marginBottom: 12,
  },
  contentInput: {
    borderWidth: 1,
    borderColor: '#ddd',
    borderRadius: 8,
    padding: 12,
    fontSize: 16,
    flex: 1,
    minHeight: 200,
  },
  saveButton: {
    backgroundColor: '#007AFF',
    padding: 14,
    borderRadius: 8,
    alignItems: 'center',
    marginTop: 16,
  },
  saveButtonText: {
    color: '#fff',
    fontSize: 16,
    fontWeight: '600',
  },
});
