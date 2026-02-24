import { CalendarEvent, Note } from '../types';
import { getCredential } from './keyring';

export interface CalDavClient {
  serverUrl: string;
  username: string;
  password: string;
}

async function getClient(): Promise<CalDavClient | null> {
  const serverUrl = await getCredential('caldav_server');
  const username = await getCredential('caldav_username');
  const password = await getCredential('caldav_password');

  if (!serverUrl || !username || !password) {
    return null;
  }

  return { serverUrl, username, password };
}

function createAuthHeader(username: string, password: string): string {
  const credentials = `${username}:${password}`;
  return `Basic ${btoa(credentials)}`;
}

export async function isConfigured(): Promise<boolean> {
  const client = await getClient();
  return client !== null;
}

export async function testConnection(): Promise<{ success: boolean; error?: string }> {
  const client = await getClient();
  if (!client) {
    return { success: false, error: 'CalDAV credentials not configured' };
  }

  try {
    const response = await fetch(client.serverUrl, {
      method: 'PROPFIND',
      headers: {
        Authorization: createAuthHeader(client.username, client.password),
        Depth: '0',
        'Content-Type': 'application/xml',
      },
      body: `<?xml version="1.0" encoding="UTF-8"?>
        <d:propfind xmlns:d="DAV:">
          <d:prop><d:current-user-principal/></d:prop>
        </d:propfind>`,
    });

    if (response.ok || response.status === 207) {
      return { success: true };
    }
    return { success: false, error: `HTTP ${response.status}` };
  } catch (error) {
    return { success: false, error: error instanceof Error ? error.message : 'Unknown error' };
  }
}

export async function fetchCalendars(): Promise<string[]> {
  const client = await getClient();
  if (!client) return [];

  try {
    const response = await fetch(client.serverUrl, {
      method: 'PROPFIND',
      headers: {
        Authorization: createAuthHeader(client.username, client.password),
        Depth: '1',
        'Content-Type': 'application/xml',
      },
      body: `<?xml version="1.0" encoding="UTF-8"?>
        <d:propfind xmlns:d="DAV:" xmlns:c="urn:ietf:params:xml:ns:caldav">
          <d:prop>
            <d:displayname/>
            <d:resourcetype/>
          </d:prop>
        </d:propfind>`,
    });

    if (!response.ok && response.status !== 207) {
      return [];
    }

    const text = await response.text();
    const calendars: string[] = [];
    const matches = text.matchAll(/<d:displayname>([^<]+)<\/d:displayname>/gi);
    for (const match of matches) {
      calendars.push(match[1]);
    }
    return calendars;
  } catch {
    return [];
  }
}

export async function fetchEvents(calendarPath?: string): Promise<CalendarEvent[]> {
  const client = await getClient();
  if (!client) return [];

  const url = calendarPath ? `${client.serverUrl}${calendarPath}` : client.serverUrl;

  try {
    const now = new Date();
    const startOfMonth = new Date(now.getFullYear(), now.getMonth(), 1);
    const endOfMonth = new Date(now.getFullYear(), now.getMonth() + 2, 0);

    const response = await fetch(url, {
      method: 'REPORT',
      headers: {
        Authorization: createAuthHeader(client.username, client.password),
        Depth: '1',
        'Content-Type': 'application/xml',
      },
      body: `<?xml version="1.0" encoding="UTF-8"?>
        <c:calendar-query xmlns:d="DAV:" xmlns:c="urn:ietf:params:xml:ns:caldav">
          <d:prop>
            <d:getetag/>
            <c:calendar-data/>
          </d:prop>
          <c:filter>
            <c:comp-filter name="VCALENDAR">
              <c:comp-filter name="VEVENT">
                <c:time-range start="${formatICalDate(startOfMonth)}" end="${formatICalDate(endOfMonth)}"/>
              </c:comp-filter>
            </c:comp-filter>
          </c:filter>
        </c:calendar-query>`,
    });

    if (!response.ok && response.status !== 207) {
      return [];
    }

    const text = await response.text();
    return parseICalEvents(text);
  } catch {
    return [];
  }
}

function formatICalDate(date: Date): string {
  return date.toISOString().replace(/[-:]/g, '').split('.')[0] + 'Z';
}

function parseICalEvents(xmlText: string): CalendarEvent[] {
  const events: CalendarEvent[] = [];
  const calDataMatches = xmlText.matchAll(/<c:calendar-data[^>]*>([^<]+)<\/c:calendar-data>/gi);

  for (const match of calDataMatches) {
    const ical = match[1]
      .replace(/&lt;/g, '<')
      .replace(/&gt;/g, '>')
      .replace(/&amp;/g, '&');

    const uid = extractICalProp(ical, 'UID');
    const summary = extractICalProp(ical, 'SUMMARY');
    const dtstart = extractICalProp(ical, 'DTSTART');
    const dtend = extractICalProp(ical, 'DTEND');
    const description = extractICalProp(ical, 'DESCRIPTION');
    const location = extractICalProp(ical, 'LOCATION');

    if (uid && summary && dtstart) {
      events.push({
        uid,
        summary,
        start: parseICalDate(dtstart),
        end: dtend ? parseICalDate(dtend) : parseICalDate(dtstart),
        description: description || undefined,
        location: location || undefined,
      });
    }
  }

  return events;
}

function extractICalProp(ical: string, prop: string): string | null {
  const regex = new RegExp(`${prop}[^:]*:([^\\r\\n]+)`, 'i');
  const match = ical.match(regex);
  return match ? match[1].trim() : null;
}

function parseICalDate(dateStr: string): Date {
  // Handle formats: 20240115T100000Z or 20240115
  const cleaned = dateStr.replace(/[^0-9TZ]/g, '');
  if (cleaned.length >= 8) {
    const year = parseInt(cleaned.slice(0, 4));
    const month = parseInt(cleaned.slice(4, 6)) - 1;
    const day = parseInt(cleaned.slice(6, 8));
    const hour = cleaned.length >= 11 ? parseInt(cleaned.slice(9, 11)) : 0;
    const minute = cleaned.length >= 13 ? parseInt(cleaned.slice(11, 13)) : 0;
    return new Date(Date.UTC(year, month, day, hour, minute));
  }
  return new Date(dateStr);
}

// Notes support via VJOURNAL or plain text files
export async function fetchNotes(): Promise<Note[]> {
  // Simplified: In production, query for VJOURNAL entries
  // For now, return empty array - implement based on server support
  return [];
}

export async function createNote(title: string, content: string): Promise<Note | null> {
  const client = await getClient();
  if (!client) return null;

  const uid = `note-${Date.now()}@cross-dashboard`;
  const now = new Date();

  const note: Note = {
    uid,
    title,
    content,
    createdAt: now,
    updatedAt: now,
  };

  // In production: PUT VJOURNAL to CalDAV server
  return note;
}
