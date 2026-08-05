# Cross Dashboard

This is an experimental client for self-hosting services like CalDAV, [Gitea](https://gitea.io), [Memos](https://usememos.com), [Nextcloud](https://nextcloud.com), etc.

90% coded by Cursor (Sonnet 4.6, GPT 5.4 and Kimi K2.5)

|Mac (Swift UI)|Android (Material 3 phone+tablet)| Linux (GTK) |
|:-:|:-:|:-:|
|<img width="615" height="342" alt="Bildschirmfoto 2026-04-15 um 02 13 33" src="https://github.com/user-attachments/assets/d95f2240-5ae0-4d88-af4d-59cdf8111036" />|<img width="478" height="350" alt="Screenshot_20260415-021127_Cross Dashboard Redacted" src="https://github.com/user-attachments/assets/e16a6862-a693-4f42-872e-ea50dcd4b7e6" />|<img width="481" height="342" alt="Bildschirmfoto 2026-05-08 um 22 02 55" src="https://github.com/user-attachments/assets/e80f946b-22c3-4c7e-aa9a-f9c3b2e87d68" />|



## Features

- Calendar sync and event management (VEVENT)
- Task management with subtasks and smart input (VTODO)
- Note-taking with CalDAV backend (VJOURNAL)
- Issue tracking across Gitea repositories (With comments and attachments)
- Quick-capture notes via a self-hosted [Memos](https://usememos.com) server (with smart event and task creation)
- Pomodoro timer
- Widgets on macOS and Android
- Markdown (Block LaTeX) rendering

## Platform Strategy

| Platform | Status | Directory | Stack |
|---|---|---|---|
| **Android** | ✅ Complete | `native-android/` | Kotlin + Jetpack Compose |
| **macOS** | ✅ Complete | `native-macos/` | Swift 6.2 + SwiftUI |
| **Linux** | ✅ Complete | `native-linux/` | GTK3 + libhandy-1 |

There is no React Native, Expo, or Electron shell in this repo; only the native trees above are maintained.

## URL Scheme

On macOS, you can quickly capture text to Memos by using the URL scheme:
```
crossdashboard://capture?text=<text>
```

with popclip plugin
```yaml
#popclip extension to Capture
name: Capture
icon: square o
url: crossdashboard://capture?text={popclip text}
```

> [!NOTE]
> This is mainly a personal project for myself, therefore, only Android 16+ and Mac OS 15+ are supported, with no plan to support older versions.

## Linux CLI and background service

Linux version installs `cross-dashboard-cli` alongside the GUI application.

```bash
# Add task
cross-dashboard-cli task '!!! deploy hotfix #work tomorrow morning'
echo 'buy milk #errands tonight' | cross-dashboard-cli task

# Capture to Memos
echo 'Follow up with Alice' | cross-dashboard-cli capture

# Cached listings; active/upcoming/open items are the default
cross-dashboard-cli list tasks
cross-dashboard-cli list events --json
cross-dashboard-cli list issues --all

# Fuzzel/dmenu pipelines
cross-dashboard-cli list task --fuzzel | fuzzel --dmenu --with-nth=2

# Force the background service to sync now
cross-dashboard-cli sync

# Pomo timer
cross-dashboard-cli pomo task 'deploy hotfix'
# With fuzzel
cross-dashboard-cli pomo fuzzel
# Control
cross-dashboard-cli pomo status/pause/resume/stop
```

### Waybar module

```json
"custom/cross-dashboard": {
    "exec": "cross-dashboard-cli waybar",
    "return-type": "json",
    "escape": true,
    "tooltip": true,
    "exec-on-event": false,
    "on-click": "cross-dashboard-cli pomo toggle",
    "on-click-middle": "cross-dashboard",
    "on-click-right": "cross-dashboard-cli sync"
  }
```

### Systemd service

```bash
# To start the backgroud service or to refresh the service
systemctl --user daemon-reload
systemctl --user enable --now crossdashboard.service
systemctl --user status crossdashboard.service
journalctl --user -u crossdashboard.service
```

### Filtered backgrounds

Inbox and Views include a camera button that snapshots the current filters and layout as the
background template. Android exposes it as a light/dark live wallpaper, macOS maintains matching
light/dark desktop images per connected display, and native Linux renders
`$XDG_CACHE_HOME/crossdashboard/background.png` from the systemd user service.

Linux accepts any direct command template containing `%f`, which is replaced by the rendered file
path. Presets are provided for `xwallpaper --zoom %f` and `swaybg -o "*" -i %f -m fill`. Commands
are parsed as arguments and are never run through a shell, so pipes, redirects, and shell expansion
are intentionally unsupported. Automatic Linux backgrounds are unavailable in the Flatpak build.
Backgrounds can expose item titles behind desktop icons or on an Android lock screen; choose the
captured filters with the same care as other lock-screen content.

Shared under the MIT license. (If AI code is licensable:))
