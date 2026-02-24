import React from 'react';
import {
  View,
  Text,
  StyleSheet,
  Modal,
  TouchableOpacity,
} from 'react-native';
import { useTheme } from '../hooks/useTheme';
import { CalDavCalendar } from '../types';
import AppIcon, { Icons } from './Icon';

interface PropertyPageModalProps {
  visible: boolean;
  children: React.ReactNode;
}

export function PropertyPageModal({ visible, children }: PropertyPageModalProps) {
  const theme = useTheme();
  return (
    <Modal visible={visible} animationType="slide" transparent>
      <View style={styles.overlay}>
        <View style={[styles.container, { backgroundColor: theme.colors.surface }]}>
          {children}
        </View>
      </View>
    </Modal>
  );
}

interface PropertyPageHeaderProps {
  title: string;
  onClose: () => void;
  canEdit?: boolean;
  isEditing?: boolean;
  onToggleEdit?: () => void;
}

export function PropertyPageHeader({ title, onClose, canEdit, isEditing, onToggleEdit }: PropertyPageHeaderProps) {
  const theme = useTheme();
  const c = theme.colors;
  return (
    <View style={[styles.header, { borderBottomColor: c.border }]}>
      <TouchableOpacity onPress={onClose} style={styles.headerButton}>
        <AppIcon name={Icons.close} size={22} color={c.text} />
      </TouchableOpacity>
      <Text style={[styles.headerTitle, { color: c.text }]} numberOfLines={1}>
        {title}
      </Text>
      {canEdit && onToggleEdit ? (
        <TouchableOpacity onPress={onToggleEdit} style={styles.headerButton}>
          {isEditing ? (
            <Text style={{ color: c.primary, fontSize: 15, fontWeight: '600' }}>Cancel</Text>
          ) : (
            <AppIcon name={Icons.pencil} size={20} color={c.primary} />
          )}
        </TouchableOpacity>
      ) : (
        <View style={styles.headerButton} />
      )}
    </View>
  );
}

interface ReadFieldProps {
  label: string;
  value?: string | null;
  valueColor?: string;
}

export function ReadField({ label, value, valueColor }: ReadFieldProps) {
  const theme = useTheme();
  if (!value) return null;
  return (
    <View style={styles.fieldRow}>
      <Text style={[styles.fieldLabel, { color: theme.colors.textSecondary }]}>{label}</Text>
      <Text style={[styles.fieldValue, { color: valueColor || theme.colors.text }]}>{value}</Text>
    </View>
  );
}

interface SectionHeaderProps {
  title: string;
}

export function SectionHeader({ title }: SectionHeaderProps) {
  const theme = useTheme();
  return (
    <Text style={[styles.sectionHeader, { color: theme.colors.textSecondary }]}>{title}</Text>
  );
}

export function resolveCalendar(
  calendarHref: string | undefined,
  calendars: CalDavCalendar[]
): { displayName: string; color?: string } | null {
  if (!calendarHref) return null;
  const cal = calendars.find((c) => c.href === calendarHref);
  if (!cal) return null;
  return { displayName: cal.displayName, color: cal.color };
}

const styles = StyleSheet.create({
  overlay: {
    flex: 1,
    backgroundColor: 'rgba(0,0,0,0.5)',
    justifyContent: 'flex-end',
  },
  container: {
    borderTopLeftRadius: 16,
    borderTopRightRadius: 16,
    maxHeight: '92%',
    minHeight: '50%',
  },
  header: {
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'space-between',
    paddingHorizontal: 16,
    paddingVertical: 14,
    borderBottomWidth: 1,
  },
  headerButton: {
    width: 60,
    alignItems: 'center',
  },
  headerTitle: {
    flex: 1,
    textAlign: 'center',
    fontSize: 17,
    fontWeight: '600',
  },
  fieldRow: {
    paddingVertical: 10,
    paddingHorizontal: 20,
  },
  fieldLabel: {
    fontSize: 12,
    fontWeight: '600',
    textTransform: 'uppercase',
    marginBottom: 4,
  },
  fieldValue: {
    fontSize: 15,
    lineHeight: 22,
  },
  sectionHeader: {
    fontSize: 13,
    fontWeight: '700',
    textTransform: 'uppercase',
    paddingHorizontal: 20,
    paddingTop: 20,
    paddingBottom: 8,
  },
});
