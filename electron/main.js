import { app, BrowserWindow } from 'electron';
import path from 'path';
import serve from 'electron-serve';

const loadURL = serve({ directory: 'dist' });

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
