// CalDAV Types
export interface CalDavConfig {
  serverUrl: string;
  username: string;
}

export interface CalendarEvent {
  uid: string;
  summary: string;
  start: Date;
  end: Date;
  description?: string;
  location?: string;
  calendar?: string;
}

export interface Note {
  uid: string;
  title: string;
  content: string;
  createdAt: Date;
  updatedAt: Date;
  tags?: string[];
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
