import { CalendarEvent, Note, CalDavTask, CalDavCalendar, TaskStatus } from '../types';
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

/**
 * Resolve a calendar href (absolute path) to a full URL using the server origin.
 * If href is already a full URL, return it as-is.
 */
function resolveHref(serverUrl: string, href: string): string {
  if (href.startsWith('http://') || href.startsWith('https://')) {
    return href;
  }
  const url = new URL(serverUrl);
  return `${url.origin}${href}`;
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

export async function fetchCalendars(): Promise<CalDavCalendar[]> {
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
        <d:propfind xmlns:d="DAV:" xmlns:c="urn:ietf:params:xml:ns:caldav"
                    xmlns:apple="http://apple.com/ns/ical/"
                    xmlns:cs="http://calendarserver.org/ns/">
          <d:prop>
            <d:displayname/>
            <d:resourcetype/>
            <apple:calendar-color/>
            <cs:getctag/>
            <c:supported-calendar-component-set/>
          </d:prop>
        </d:propfind>`,
    });

    if (!response.ok && response.status !== 207) {
      return [];
    }

    const text = await response.text();
    return parseCalendarList(text);
  } catch {
    return [];
  }
}

function parseCalendarList(xmlText: string): CalDavCalendar[] {
  const calendars: CalDavCalendar[] = [];

  // Split into individual responses — handle any namespace prefix (d:, D:, etc.)
  const responseBlocks = xmlText.split(/<(?:\w+:)?response>/gi).slice(1);

  for (const block of responseBlocks) {
    // Must be a calendar collection (has both <X:collection/> and <X:calendar/>)
    // Use prefix-agnostic regex: namespace prefix can be any word chars followed by colon
    const hasCollection = /<(?:\w+:)?collection[\s/>]/i.test(block);
    const hasCalendar = /<(?:\w+:)?calendar[\s/>]/i.test(block);
    if (!hasCollection || !hasCalendar) continue;

    // Extract href
    const hrefMatch = block.match(/<(?:\w+:)?href>([^<]+)<\/(?:\w+:)?href>/i);
    if (!hrefMatch) continue;
    const href = hrefMatch[1];

    // Extract display name
    const nameMatch = block.match(/<(?:\w+:)?displayname>([^<]+)<\/(?:\w+:)?displayname>/i);
    const displayName = nameMatch ? nameMatch[1] : href.split('/').filter(Boolean).pop() || 'Unknown';

    // Extract color (strip alpha from #RRGGBBAA → #RRGGBB)
    // Server may use any prefix for apple namespace (apple:, x1:, x2:, etc.)
    let color: string | undefined;
    const colorMatch = block.match(/<(?:\w+:)?calendar-color>([^<]+)<\/(?:\w+:)?calendar-color>/i);
    if (colorMatch) {
      let c = colorMatch[1].trim();
      if (c.length === 9 && c.startsWith('#')) {
        c = c.slice(0, 7);
      }
      color = c;
    }

    // Extract ctag
    let ctag: string | undefined;
    const ctagMatch = block.match(/<(?:\w+:)?getctag>([^<]+)<\/(?:\w+:)?getctag>/i);
    if (ctagMatch) ctag = ctagMatch[1];

    // Extract supported components
    const components: string[] = [];
    const compMatches = block.matchAll(/<(?:\w+:)?comp\s+name="([^"]+)"/gi);
    for (const cm of compMatches) {
      components.push(cm[1].toUpperCase());
    }
    // If no component info, assume VEVENT
    if (components.length === 0) {
      components.push('VEVENT');
    }

    calendars.push({ href, displayName, color, ctag, components });
  }

  return calendars;
}

export async function fetchEvents(calendarHrefs?: string[]): Promise<CalendarEvent[]> {
  const client = await getClient();
  if (!client) return [];

  const urls = calendarHrefs && calendarHrefs.length > 0
    ? calendarHrefs.map((href) => resolveHref(client.serverUrl, href))
    : [client.serverUrl];

  const now = new Date();
  const startOfMonth = new Date(now.getFullYear(), now.getMonth(), 1);
  const endOfMonth = new Date(now.getFullYear(), now.getMonth() + 2, 0);

  const allEvents: CalendarEvent[] = [];

  // Build a map from URL to href for tagging
  const urlToHref = new Map<string, string>();
  if (calendarHrefs && calendarHrefs.length > 0) {
    for (const href of calendarHrefs) {
      urlToHref.set(resolveHref(client.serverUrl, href), href);
    }
  }

  for (const url of urls) {
    try {
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

      if (response.ok || response.status === 207) {
        const text = await response.text();
        const events = parseICalEvents(text);
        const href = urlToHref.get(url);
        if (href) {
          for (const ev of events) ev.calendarHref = href;
        }
        allEvents.push(...events);
      }
    } catch {
      // skip failed calendars
    }
  }

  return allEvents;
}

function formatICalDate(date: Date): string {
  return date.toISOString().replace(/[-:]/g, '').split('.')[0] + 'Z';
}

function parseICalEvents(xmlText: string): CalendarEvent[] {
  const events: CalendarEvent[] = [];
  const calDataMatches = xmlText.matchAll(/<(?:\w+:)?calendar-data[^>]*>([^<]+)<\/(?:\w+:)?calendar-data>/gi);

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

// Notes support via VJOURNAL
export async function fetchNotes(calendarHrefs?: string[]): Promise<Note[]> {
  const client = await getClient();
  if (!client) return [];

  const urls = calendarHrefs && calendarHrefs.length > 0
    ? calendarHrefs.map((href) => resolveHref(client.serverUrl, href))
    : [client.serverUrl];

  const allNotes: Note[] = [];

  for (const url of urls) {
    try {
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
                <c:comp-filter name="VJOURNAL"/>
              </c:comp-filter>
            </c:filter>
          </c:calendar-query>`,
      });

      if (response.ok || response.status === 207) {
        const text = await response.text();
        allNotes.push(...parseVJournalEntries(text));
      }
    } catch {
      // skip failed calendars
    }
  }

  return allNotes.sort((a, b) => b.updatedAt.getTime() - a.updatedAt.getTime());
}

function parseVJournalEntries(xmlText: string): Note[] {
  const notes: Note[] = [];
  const calDataMatches = xmlText.matchAll(/<(?:\w+:)?calendar-data[^>]*>([^<]+)<\/(?:\w+:)?calendar-data>/gi);

  for (const match of calDataMatches) {
    const ical = match[1]
      .replace(/&lt;/g, '<')
      .replace(/&gt;/g, '>')
      .replace(/&amp;/g, '&');

    const uid = extractICalProp(ical, 'UID');
    const summary = extractICalProp(ical, 'SUMMARY');
    const description = extractICalProp(ical, 'DESCRIPTION');
    const dtstart = extractICalProp(ical, 'DTSTART');
    const lastModified = extractICalProp(ical, 'LAST-MODIFIED');
    const categories = extractICalProp(ical, 'CATEGORIES');

    if (uid && summary) {
      notes.push({
        uid,
        title: summary,
        content: description || '',
        createdAt: dtstart ? parseICalDate(dtstart) : new Date(),
        updatedAt: lastModified ? parseICalDate(lastModified) : new Date(),
        tags: categories ? categories.split(',').map((t) => t.trim()) : undefined,
      });
    }
  }

  return notes.sort((a, b) => b.updatedAt.getTime() - a.updatedAt.getTime());
}

function buildVJournal(uid: string, title: string, content: string, createdAt: Date, tags?: string[]): string {
  const now = new Date();
  let vjournal = `BEGIN:VCALENDAR\r\nVERSION:2.0\r\nPRODID:-//CrossDashboard//EN\r\nBEGIN:VJOURNAL\r\nUID:${uid}\r\nDTSTAMP:${formatICalDate(now)}\r\nDTSTART:${formatICalDate(createdAt)}\r\nLAST-MODIFIED:${formatICalDate(now)}\r\nSUMMARY:${title}\r\n`;

  if (content) {
    const escaped = content.replace(/\\/g, '\\\\').replace(/\n/g, '\\n').replace(/,/g, '\\,').replace(/;/g, '\\;');
    vjournal += `DESCRIPTION:${escaped}\r\n`;
  }

  if (tags && tags.length > 0) {
    vjournal += `CATEGORIES:${tags.join(',')}\r\n`;
  }

  vjournal += `END:VJOURNAL\r\nEND:VCALENDAR`;
  return vjournal;
}

export async function createNote(title: string, content: string, tags?: string[], calendarHref?: string): Promise<Note | null> {
  const client = await getClient();
  if (!client) return null;

  const uid = `note-${Date.now()}@cross-dashboard`;
  const now = new Date();
  const vjournal = buildVJournal(uid, title, content, now, tags);
  const baseUrl = calendarHref ? resolveHref(client.serverUrl, calendarHref) : client.serverUrl;

  try {
    const response = await fetch(`${baseUrl.replace(/\/+$/, '')}/${uid}.ics`, {
      method: 'PUT',
      headers: {
        Authorization: createAuthHeader(client.username, client.password),
        'Content-Type': 'text/calendar; charset=utf-8',
        'If-None-Match': '*',
      },
      body: vjournal,
    });

    if (!response.ok && response.status !== 201 && response.status !== 204) {
      return null;
    }

    return { uid, title, content, createdAt: now, updatedAt: now, tags };
  } catch {
    return null;
  }
}

export async function updateNote(uid: string, title: string, content: string, createdAt: Date, tags?: string[]): Promise<boolean> {
  const client = await getClient();
  if (!client) return false;

  const vjournal = buildVJournal(uid, title, content, createdAt, tags);

  try {
    const response = await fetch(`${client.serverUrl}/${uid}.ics`, {
      method: 'PUT',
      headers: {
        Authorization: createAuthHeader(client.username, client.password),
        'Content-Type': 'text/calendar; charset=utf-8',
      },
      body: vjournal,
    });

    return response.ok || response.status === 204;
  } catch {
    return false;
  }
}

export async function deleteNote(uid: string): Promise<boolean> {
  const client = await getClient();
  if (!client) return false;

  try {
    const response = await fetch(`${client.serverUrl}/${uid}.ics`, {
      method: 'DELETE',
      headers: {
        Authorization: createAuthHeader(client.username, client.password),
      },
    });

    return response.ok || response.status === 204;
  } catch {
    return false;
  }
}

// VTODO (Tasks) support

export async function fetchTasks(calendarHrefs?: string[]): Promise<CalDavTask[]> {
  const client = await getClient();
  if (!client) return [];

  const urls = calendarHrefs && calendarHrefs.length > 0
    ? calendarHrefs.map((href) => resolveHref(client.serverUrl, href))
    : [client.serverUrl];

  const allTasks: CalDavTask[] = [];

  // Build a map from URL to href for tagging
  const urlToHref = new Map<string, string>();
  if (calendarHrefs && calendarHrefs.length > 0) {
    for (const href of calendarHrefs) {
      urlToHref.set(resolveHref(client.serverUrl, href), href);
    }
  }

  for (const url of urls) {
    try {
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
                <c:comp-filter name="VTODO"/>
              </c:comp-filter>
            </c:filter>
          </c:calendar-query>`,
      });

      if (response.ok || response.status === 207) {
        const text = await response.text();
        const tasks = parseVTodoEntries(text);
        const href = urlToHref.get(url);
        if (href) {
          for (const t of tasks) t.calendarHref = href;
        }
        allTasks.push(...tasks);
      }
    } catch {
      // skip failed calendars
    }
  }

  return allTasks.sort((a, b) => b.lastModified.getTime() - a.lastModified.getTime());
}

function parseVTodoEntries(xmlText: string): CalDavTask[] {
  const tasks: CalDavTask[] = [];
  const calDataMatches = xmlText.matchAll(/<(?:\w+:)?calendar-data[^>]*>([^<]+)<\/(?:\w+:)?calendar-data>/gi);

  for (const match of calDataMatches) {
    const ical = match[1]
      .replace(/&lt;/g, '<')
      .replace(/&gt;/g, '>')
      .replace(/&amp;/g, '&');

    const uid = extractICalProp(ical, 'UID');
    const summary = extractICalProp(ical, 'SUMMARY');
    if (!uid || !summary) continue;

    const description = extractICalProp(ical, 'DESCRIPTION');
    const status = (extractICalProp(ical, 'STATUS') || 'NEEDS-ACTION') as TaskStatus;
    const priorityStr = extractICalProp(ical, 'PRIORITY');
    const percentStr = extractICalProp(ical, 'PERCENT-COMPLETE');
    const due = extractICalProp(ical, 'DUE');
    const dtstart = extractICalProp(ical, 'DTSTART');
    const completed = extractICalProp(ical, 'COMPLETED');
    const created = extractICalProp(ical, 'CREATED');
    const lastModified = extractICalProp(ical, 'LAST-MODIFIED');
    const categories = extractICalProp(ical, 'CATEGORIES');
    const location = extractICalProp(ical, 'LOCATION');

    // Parse RELATED-TO with RELTYPE=PARENT or bare RELATED-TO (default is PARENT)
    const parentMatch = ical.match(/RELATED-TO;RELTYPE=PARENT:([^\r\n]+)/i)
      || ical.match(/RELATED-TO:([^\r\n]+)/i);
    const parentUid = parentMatch ? parentMatch[1].trim() : undefined;

    tasks.push({
      uid,
      summary,
      description: description || undefined,
      status,
      priority: priorityStr ? parseInt(priorityStr, 10) : 0,
      percentComplete: percentStr ? parseInt(percentStr, 10) : 0,
      due: due ? parseICalDate(due) : undefined,
      dtstart: dtstart ? parseICalDate(dtstart) : undefined,
      completed: completed ? parseICalDate(completed) : undefined,
      created: created ? parseICalDate(created) : new Date(),
      lastModified: lastModified ? parseICalDate(lastModified) : new Date(),
      categories: categories ? categories.split(',').map((t) => t.trim()) : undefined,
      location: location || undefined,
      parentUid,
    });
  }

  return tasks.sort((a, b) => b.lastModified.getTime() - a.lastModified.getTime());
}

function buildVTodo(task: CalDavTask): string {
  const now = new Date();
  let vtodo = `BEGIN:VCALENDAR\r\nVERSION:2.0\r\nPRODID:-//CrossDashboard//EN\r\nBEGIN:VTODO\r\nUID:${task.uid}\r\nDTSTAMP:${formatICalDate(now)}\r\nCREATED:${formatICalDate(task.created)}\r\nLAST-MODIFIED:${formatICalDate(now)}\r\nSUMMARY:${task.summary}\r\nSTATUS:${task.status}\r\nPRIORITY:${task.priority}\r\nPERCENT-COMPLETE:${task.percentComplete}\r\n`;

  if (task.description) {
    const escaped = task.description.replace(/\\/g, '\\\\').replace(/\n/g, '\\n').replace(/,/g, '\\,').replace(/;/g, '\\;');
    vtodo += `DESCRIPTION:${escaped}\r\n`;
  }

  if (task.due) {
    vtodo += `DUE:${formatICalDate(task.due)}\r\n`;
  }

  if (task.dtstart) {
    vtodo += `DTSTART:${formatICalDate(task.dtstart)}\r\n`;
  }

  if (task.completed) {
    vtodo += `COMPLETED:${formatICalDate(task.completed)}\r\n`;
  }

  if (task.categories && task.categories.length > 0) {
    vtodo += `CATEGORIES:${task.categories.join(',')}\r\n`;
  }

  if (task.location) {
    vtodo += `LOCATION:${task.location}\r\n`;
  }

  if (task.parentUid) {
    vtodo += `RELATED-TO;RELTYPE=PARENT:${task.parentUid}\r\n`;
  }

  vtodo += `END:VTODO\r\nEND:VCALENDAR`;
  return vtodo;
}

export async function createTask(task: Omit<CalDavTask, 'uid' | 'created' | 'lastModified'>, calendarHref?: string): Promise<CalDavTask | null> {
  const client = await getClient();
  if (!client) return null;

  const now = new Date();
  const uid = `task-${Date.now()}@cross-dashboard`;
  const fullTask: CalDavTask = { ...task, uid, created: now, lastModified: now };
  const vtodo = buildVTodo(fullTask);
  const baseUrl = calendarHref ? resolveHref(client.serverUrl, calendarHref) : client.serverUrl;

  try {
    const response = await fetch(`${baseUrl.replace(/\/+$/, '')}/${uid}.ics`, {
      method: 'PUT',
      headers: {
        Authorization: createAuthHeader(client.username, client.password),
        'Content-Type': 'text/calendar; charset=utf-8',
        'If-None-Match': '*',
      },
      body: vtodo,
    });

    if (!response.ok && response.status !== 201 && response.status !== 204) {
      return null;
    }

    return fullTask;
  } catch {
    return null;
  }
}

export async function updateTask(task: CalDavTask): Promise<boolean> {
  const client = await getClient();
  if (!client) return false;

  const vtodo = buildVTodo(task);

  try {
    const response = await fetch(`${client.serverUrl}/${task.uid}.ics`, {
      method: 'PUT',
      headers: {
        Authorization: createAuthHeader(client.username, client.password),
        'Content-Type': 'text/calendar; charset=utf-8',
      },
      body: vtodo,
    });

    return response.ok || response.status === 204;
  } catch {
    return false;
  }
}

export async function deleteTask(uid: string): Promise<boolean> {
  const client = await getClient();
  if (!client) return false;

  try {
    const response = await fetch(`${client.serverUrl}/${uid}.ics`, {
      method: 'DELETE',
      headers: {
        Authorization: createAuthHeader(client.username, client.password),
      },
    });

    return response.ok || response.status === 204;
  } catch {
    return false;
  }
}
