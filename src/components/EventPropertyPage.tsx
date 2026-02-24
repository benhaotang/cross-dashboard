import React from 'react';
import { View, Text, StyleSheet, ScrollView } from 'react-native';
import { useTheme } from '../hooks/useTheme';
import { CalendarEvent, CalDavCalendar } from '../types';
import {
  PropertyPageModal,
  PropertyPageHeader,
  ReadField,
  SectionHeader,
  resolveCalendar,
} from './PropertyPageShared';

interface Props {
  event: CalendarEvent;
  calendars: CalDavCalendar[];
  onClose: () => void;
}

function formatFullRange(start: Date, end: Date): string {
  const opts: Intl.DateTimeFormatOptions = {
    weekday: 'short',
    month: 'short',
    day: 'numeric',
    year: 'numeric',
    hour: '2-digit',
    minute: '2-digit',
  };
  return `${start.toLocaleString(undefined, opts)}  \u2192  ${end.toLocaleTimeString(undefined, { hour: '2-digit', minute: '2-digit' })}`;
}

function formatDuration(start: Date, end: Date): string {
  const ms = end.getTime() - start.getTime();
  const mins = Math.round(ms / 60000);
  if (mins < 60) return `${mins}m`;
  const h = Math.floor(mins / 60);
  const m = mins % 60;
  return m > 0 ? `${h}h ${m}m` : `${h}h`;
}

export default function EventPropertyPage({ event, calendars, onClose }: Props) {
  const theme = useTheme();
  const c = theme.colors;
  const cal = resolveCalendar(event.calendarHref, calendars);

  return (
    <PropertyPageModal visible>
      <PropertyPageHeader title="Event" onClose={onClose} canEdit={false} />
      <ScrollView style={styles.body} contentContainerStyle={styles.bodyContent}>
        <Text style={[styles.summary, { color: c.text }]}>{event.summary}</Text>

        {cal && (
          <View style={styles.calRow}>
            {cal.color && <View style={[styles.calDot, { backgroundColor: cal.color }]} />}
            <Text style={[styles.calName, { color: c.textSecondary }]}>{cal.displayName}</Text>
          </View>
        )}

        <ReadField label="Date & Time" value={formatFullRange(event.start, event.end)} />
        <ReadField label="Duration" value={formatDuration(event.start, event.end)} />
        {event.location && <ReadField label="Location" value={event.location} />}

        {event.description ? (
          <>
            <SectionHeader title="Description" />
            <Text style={[styles.descText, { color: c.text }]}>{event.description}</Text>
          </>
        ) : null}

        <SectionHeader title="Metadata" />
        <ReadField label="UID" value={event.uid} valueColor={c.textTertiary} />
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
  summary: {
    fontSize: 22,
    fontWeight: '700',
    paddingHorizontal: 20,
    paddingTop: 20,
    paddingBottom: 8,
  },
  calRow: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 8,
    paddingHorizontal: 20,
    paddingBottom: 12,
  },
  calDot: {
    width: 12,
    height: 12,
    borderRadius: 6,
  },
  calName: {
    fontSize: 14,
  },
  descText: {
    fontSize: 15,
    lineHeight: 22,
    paddingHorizontal: 20,
  },
});
