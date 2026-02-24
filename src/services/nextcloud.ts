// Nextcloud Login Flow v2
// https://docs.nextcloud.com/server/latest/developer_manual/client_apis/LoginFlow/index.html#login-flow-v2

export interface LoginFlowInit {
  pollToken: string;
  pollEndpoint: string;
  loginUrl: string;
}

export interface LoginFlowCredentials {
  server: string;
  loginName: string;
  appPassword: string;
}

/**
 * Step 1: Initiate Login Flow v2.
 * POST /index.php/login/v2 → { poll: { token, endpoint }, login: URL }
 */
export async function initiateLoginFlow(serverUrl: string): Promise<LoginFlowInit> {
  const base = serverUrl.replace(/\/+$/, '');
  const response = await fetch(`${base}/index.php/login/v2`, {
    method: 'POST',
    headers: { 'User-Agent': 'CrossDashboard' },
  });

  if (!response.ok) {
    throw new Error(`Login flow initiation failed: HTTP ${response.status}`);
  }

  const data = await response.json();
  return {
    pollToken: data.poll.token,
    pollEndpoint: data.poll.endpoint,
    loginUrl: data.login,
  };
}

/**
 * Step 2: Poll for credentials.
 * POST pollEndpoint with token every 2s.
 * 404 = user hasn't granted yet, 200 = { server, loginName, appPassword }
 */
export async function pollForCredentials(
  pollEndpoint: string,
  pollToken: string,
  signal?: AbortSignal,
): Promise<LoginFlowCredentials> {
  const maxAttempts = 150; // 5 minutes at 2s intervals
  for (let i = 0; i < maxAttempts; i++) {
    if (signal?.aborted) {
      throw new Error('Login flow cancelled');
    }

    const response = await fetch(pollEndpoint, {
      method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      body: `token=${encodeURIComponent(pollToken)}`,
      signal,
    });

    if (response.status === 200) {
      const data = await response.json();
      return {
        server: data.server,
        loginName: data.loginName,
        appPassword: data.appPassword,
      };
    }

    // 404 means not yet authorized — wait and retry
    if (response.status !== 404) {
      throw new Error(`Unexpected poll response: HTTP ${response.status}`);
    }

    await new Promise((resolve) => setTimeout(resolve, 2000));
  }

  throw new Error('Login flow timed out');
}

/**
 * Construct the CalDAV base URL for a Nextcloud user.
 * Returns e.g. https://cloud.example.com/remote.php/dav/calendars/alice/
 */
export function discoverCalDavUrl(serverUrl: string, username: string): string {
  const base = serverUrl.replace(/\/+$/, '');
  return `${base}/remote.php/dav/calendars/${encodeURIComponent(username)}/`;
}
