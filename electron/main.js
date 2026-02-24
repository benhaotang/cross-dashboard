import { app, BrowserWindow, session } from 'electron';
import path from 'path';
import serve from 'electron-serve';

// isCorsEnabled: true (default) is required — without it Chromium won't allow
// cross-origin fetch at all ("Failed to fetch"). Instead we intercept responses
// locally via onHeadersReceived and inject CORS approval before Chromium's CORS
// check runs. The remote server never sees these injected headers.
const loadURL = serve({ directory: 'dist' });

function bypassCors() {
  session.defaultSession.webRequest.onHeadersReceived((details, callback) => {
    const origin = details.requestHeaders?.Origin ?? details.requestHeaders?.origin ?? '*';
    callback({
      responseHeaders: {
        ...details.responseHeaders,
        'Access-Control-Allow-Origin': [origin],
        'Access-Control-Allow-Methods': ['GET, POST, PUT, DELETE, PATCH, OPTIONS, PROPFIND, REPORT, MKCALENDAR'],
        'Access-Control-Allow-Headers': ['Authorization, Content-Type, Depth, DAV, If-Match, If-None-Match, Prefer'],
        'Access-Control-Allow-Credentials': ['true'],
      },
    });
  });
}

async function createWindow() {
  const win = new BrowserWindow({
    width: 1280,
    height: 800,
    minWidth: 600,
    minHeight: 480,
    autoHideMenuBar: true,
    title: 'Cross Dashboard',
    icon: path.join(app.getAppPath(), 'assets/icon.png'),
    webPreferences: {
      nodeIntegration: false,
      contextIsolation: true,
    },
  });

  await loadURL(win);
}

app.whenReady().then(() => {
  bypassCors();
  createWindow();

  app.on('activate', () => {
    if (BrowserWindow.getAllWindows().length === 0) {
      createWindow();
    }
  });
});

app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') {
    app.quit();
  }
});
