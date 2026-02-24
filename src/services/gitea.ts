import { GiteaIssue, GiteaLabel, GiteaUser, GiteaMilestone, GiteaComment } from '../types';
import { getCredential } from './keyring';

interface GiteaClient {
  instanceUrl: string;
  token: string;
}

async function getClient(): Promise<GiteaClient | null> {
  const instanceUrl = await getCredential('gitea_instance');
  const token = await getCredential('gitea_token');

  if (!instanceUrl || !token) {
    return null;
  }

  return { instanceUrl: instanceUrl.replace(/\/$/, ''), token };
}

async function apiRequest<T>(path: string, options: RequestInit = {}): Promise<T | null> {
  const client = await getClient();
  if (!client) return null;

  try {
    const response = await fetch(`${client.instanceUrl}/api/v1${path}`, {
      ...options,
      headers: {
        Authorization: `token ${client.token}`,
        'Content-Type': 'application/json',
        ...options.headers,
      },
    });

    if (!response.ok) {
      console.error(`Gitea API error: ${response.status}`);
      return null;
    }

    return await response.json();
  } catch (error) {
    console.error('Gitea API request failed:', error);
    return null;
  }
}

export async function isConfigured(): Promise<boolean> {
  const client = await getClient();
  return client !== null;
}

export async function testConnection(): Promise<{ success: boolean; error?: string }> {
  const client = await getClient();
  if (!client) {
    return { success: false, error: 'Gitea credentials not configured' };
  }

  try {
    const response = await fetch(`${client.instanceUrl}/api/v1/user`, {
      headers: {
        Authorization: `token ${client.token}`,
      },
    });

    if (response.ok) {
      return { success: true };
    }
    return { success: false, error: `HTTP ${response.status}` };
  } catch (error) {
    return { success: false, error: error instanceof Error ? error.message : 'Unknown error' };
  }
}

export async function fetchCurrentUser(): Promise<GiteaUser | null> {
  const data = await apiRequest<{ id: number; login: string; avatar_url: string }>('/user');
  if (!data) return null;

  return {
    id: data.id,
    login: data.login,
    avatarUrl: data.avatar_url,
  };
}

export async function fetchIssues(
  owner: string,
  repo: string,
  state: 'open' | 'closed' | 'all' = 'open'
): Promise<GiteaIssue[]> {
  interface ApiIssue {
    id: number;
    number: number;
    title: string;
    body: string;
    state: string;
    labels: Array<{ id: number; name: string; color: string }>;
    assignees: Array<{ id: number; login: string; avatar_url: string }> | null;
    created_at: string;
    updated_at: string;
    html_url: string;
  }

  const data = await apiRequest<ApiIssue[]>(
    `/repos/${owner}/${repo}/issues?state=${state}&type=issues`
  );

  if (!data) return [];

  return data.map((issue) => ({
    id: issue.id,
    number: issue.number,
    title: issue.title,
    body: issue.body || '',
    state: issue.state as 'open' | 'closed',
    labels: issue.labels.map((l) => ({
      id: l.id,
      name: l.name,
      color: l.color,
    })),
    assignees: (issue.assignees || []).map((a) => ({
      id: a.id,
      login: a.login,
      avatarUrl: a.avatar_url,
    })),
    repository: `${owner}/${repo}`,
    createdAt: new Date(issue.created_at),
    updatedAt: new Date(issue.updated_at),
    htmlUrl: issue.html_url,
  }));
}

export async function fetchAllIssues(repositories: string[]): Promise<GiteaIssue[]> {
  const allIssues: GiteaIssue[] = [];

  for (const repo of repositories) {
    const [owner, repoName] = repo.split('/');
    if (owner && repoName) {
      const issues = await fetchIssues(owner, repoName);
      allIssues.push(...issues);
    }
  }

  return allIssues.sort((a, b) => b.updatedAt.getTime() - a.updatedAt.getTime());
}

export async function createIssue(
  owner: string,
  repo: string,
  title: string,
  body: string,
  labels?: string[]
): Promise<GiteaIssue | null> {
  interface ApiIssue {
    id: number;
    number: number;
    title: string;
    body: string;
    state: string;
    labels: Array<{ id: number; name: string; color: string }>;
    assignees: Array<{ id: number; login: string; avatar_url: string }> | null;
    created_at: string;
    updated_at: string;
    html_url: string;
  }

  const data = await apiRequest<ApiIssue>(`/repos/${owner}/${repo}/issues`, {
    method: 'POST',
    body: JSON.stringify({ title, body, labels }),
  });

  if (!data) return null;

  return {
    id: data.id,
    number: data.number,
    title: data.title,
    body: data.body || '',
    state: data.state as 'open' | 'closed',
    labels: data.labels.map((l) => ({
      id: l.id,
      name: l.name,
      color: l.color,
    })),
    assignees: (data.assignees || []).map((a) => ({
      id: a.id,
      login: a.login,
      avatarUrl: a.avatar_url,
    })),
    repository: `${owner}/${repo}`,
    createdAt: new Date(data.created_at),
    updatedAt: new Date(data.updated_at),
    htmlUrl: data.html_url,
  };
}

export async function updateIssueState(
  owner: string,
  repo: string,
  issueNumber: number,
  state: 'open' | 'closed'
): Promise<boolean> {
  const data = await apiRequest(`/repos/${owner}/${repo}/issues/${issueNumber}`, {
    method: 'PATCH',
    body: JSON.stringify({ state }),
  });

  return data !== null;
}

export async function fetchLabels(owner: string, repo: string): Promise<GiteaLabel[]> {
  const data = await apiRequest<Array<{ id: number; name: string; color: string }>>(
    `/repos/${owner}/${repo}/labels`
  );

  if (!data) return [];

  return data.map((l) => ({
    id: l.id,
    name: l.name,
    color: l.color,
  }));
}

export async function fetchMilestones(
  owner: string,
  repo: string,
  state: 'open' | 'closed' | 'all' = 'open'
): Promise<GiteaMilestone[]> {
  interface ApiMilestone {
    id: number;
    title: string;
    description: string;
    state: string;
    due_on: string | null;
    open_issues: number;
    closed_issues: number;
    html_url: string;
  }

  const data = await apiRequest<ApiMilestone[]>(
    `/repos/${owner}/${repo}/milestones?state=${state}`
  );

  if (!data) return [];

  return data.map((m) => ({
    id: m.id,
    title: m.title,
    description: m.description || '',
    state: m.state as 'open' | 'closed',
    dueOn: m.due_on ? new Date(m.due_on) : null,
    repository: `${owner}/${repo}`,
    openIssues: m.open_issues,
    closedIssues: m.closed_issues,
    htmlUrl: m.html_url,
  }));
}

export async function fetchAllMilestones(repositories: string[]): Promise<GiteaMilestone[]> {
  const allMilestones: GiteaMilestone[] = [];

  for (const repo of repositories) {
    const [owner, repoName] = repo.split('/');
    if (owner && repoName) {
      const milestones = await fetchMilestones(owner, repoName, 'all');
      allMilestones.push(...milestones);
    }
  }

  return allMilestones.sort((a, b) => {
    if (!a.dueOn && !b.dueOn) return 0;
    if (!a.dueOn) return 1;
    if (!b.dueOn) return -1;
    return a.dueOn.getTime() - b.dueOn.getTime();
  });
}

export async function fetchComments(
  owner: string,
  repo: string,
  issueNumber: number
): Promise<GiteaComment[]> {
  interface ApiComment {
    id: number;
    body: string;
    user: { id: number; login: string; avatar_url: string };
    created_at: string;
    updated_at: string;
  }

  const data = await apiRequest<ApiComment[]>(
    `/repos/${owner}/${repo}/issues/${issueNumber}/comments`
  );

  if (!data) return [];

  return data.map((c) => ({
    id: c.id,
    body: c.body,
    user: { id: c.user.id, login: c.user.login, avatarUrl: c.user.avatar_url },
    createdAt: new Date(c.created_at),
    updatedAt: new Date(c.updated_at),
  }));
}

export async function addComment(
  owner: string,
  repo: string,
  issueNumber: number,
  body: string
): Promise<GiteaComment | null> {
  interface ApiComment {
    id: number;
    body: string;
    user: { id: number; login: string; avatar_url: string };
    created_at: string;
    updated_at: string;
  }

  const data = await apiRequest<ApiComment>(
    `/repos/${owner}/${repo}/issues/${issueNumber}/comments`,
    { method: 'POST', body: JSON.stringify({ body }) }
  );

  if (!data) return null;

  return {
    id: data.id,
    body: data.body,
    user: { id: data.user.id, login: data.user.login, avatarUrl: data.user.avatar_url },
    createdAt: new Date(data.created_at),
    updatedAt: new Date(data.updated_at),
  };
}

export async function updateIssue(
  owner: string,
  repo: string,
  issueNumber: number,
  updates: { title?: string; body?: string; state?: 'open' | 'closed' }
): Promise<boolean> {
  const data = await apiRequest(`/repos/${owner}/${repo}/issues/${issueNumber}`, {
    method: 'PATCH',
    body: JSON.stringify(updates),
  });

  return data !== null;
}
