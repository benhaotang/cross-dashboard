// CalDAV Types
export interface CalDavConfig {
  serverUrl: string;
  username: string;
}

export interface CalDavCalendar {
  href: string;          // e.g. /remote.php/dav/calendars/user/personal/
  displayName: string;   // e.g. "Personal"
  color?: string;        // e.g. "#795AAB" (from apple:calendar-color)
  ctag?: string;         // change tag for cache invalidation
  components: string[];  // ['VEVENT', 'VTODO', 'VJOURNAL']
}

export interface CalendarEvent {
  uid: string;
  summary: string;
  start: Date;
  end: Date;
  description?: string;
  location?: string;
  calendar?: string;
  calendarHref?: string;  // href of the calendar this event was fetched from
}

export interface Note {
  uid: string;
  title: string;
  content: string;
  createdAt: Date;
  updatedAt: Date;
  tags?: string[];
}

// CalDAV Task Types
export type TaskStatus = 'NEEDS-ACTION' | 'IN-PROCESS' | 'COMPLETED' | 'CANCELLED';

export interface CalDavTask {
  uid: string;
  summary: string;
  description?: string;
  status: TaskStatus;
  priority: number;       // 0=undefined, 1-4=high, 5=medium, 6-9=low
  percentComplete: number; // 0-100
  due?: Date;
  dtstart?: Date;
  completed?: Date;        // UTC timestamp when completed
  created: Date;
  lastModified: Date;
  categories?: string[];
  location?: string;
  parentUid?: string;      // RELATED-TO;RELTYPE=PARENT value
  calendarHref?: string;   // href of the calendar this task was fetched from
}

// Gitea Types
export interface GiteaConfig {
  instanceUrl: string;
  repositories: string[];
}

export interface GiteaIssue {
  id: number;
  number: number;
  title: string;
  body: string;
  state: 'open' | 'closed';
  labels: GiteaLabel[];
  assignees: GiteaUser[];
  repository: string;
  createdAt: Date;
  updatedAt: Date;
  htmlUrl: string;
}

export interface GiteaLabel {
  id: number;
  name: string;
  color: string;
}

export interface GiteaUser {
  id: number;
  login: string;
  avatarUrl: string;
}

// Gitea Milestone
export interface GiteaMilestone {
  id: number;
  title: string;
  description: string;
  state: 'open' | 'closed';
  dueOn: Date | null;
  repository: string;
  openIssues: number;
  closedIssues: number;
  htmlUrl: string;
}

// Inbox Types
export type InboxItemType = 'event' | 'issue' | 'milestone' | 'task';

export interface InboxItem {
  id: string;
  type: InboxItemType;
  title: string;
  description?: string;
  date: Date;
  endDate?: Date;
  state?: 'open' | 'closed';
  source: string;
  sourceUrl?: string;
  labels?: GiteaLabel[];
  priority?: 'low' | 'medium' | 'high';
}

export interface InboxFilter {
  types: InboxItemType[];
  dateFrom: Date | null;
  dateTo: Date | null;
}

// App Types
export type Platform = 'android' | 'ios' | 'macos' | 'web';

export interface StoredCredential {
  key: string;
  service: 'caldav' | 'gitea';
}
