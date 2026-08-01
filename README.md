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

## Linux CalDAV defaults

The native Linux app exposes default destination calendars under **Settings → CalDAV → Defaults
for new items**. Run calendar discovery first, then choose and save:

- Default event calendar from collections supporting `VEVENT`.
- Default task calendar from collections supporting `VTODO`.
- Default note calendar from collections supporting `VJOURNAL`.

These defaults are used by task quick input, CLI task creation, new notes, Capture → Create Event,
and Capture → Extract Tasks. Saving a default also keeps that calendar in the selected sync set.
Existing saved defaults remain visible in Settings before calendars are rediscovered.

## Linux CLI and background service

Native Linux installs `cross-dashboard-cli` alongside the GTK application. It shares the app's
credentials, preferences, and SQLite cache:

```bash
# Smart task input (arguments or stdin)
cross-dashboard-cli task '!!! deploy hotfix #work tomorrow morning'
echo 'buy milk #errands tonight' | cross-dashboard-cli task

# Capture to Memos
echo 'Follow up with Alice' | cross-dashboard-cli capture

# Cached listings; active/upcoming/open items are the default
cross-dashboard-cli list tasks
cross-dashboard-cli list events --json
cross-dashboard-cli list issues --all

# Stable ID + friendly display text for Fuzzel/dmenu pipelines
cross-dashboard-cli list task --fuzzel | fuzzel --dmenu --with-nth=2

# Force the background service to sync now
cross-dashboard-cli sync

# Terminal countdown plus desktop start/completion notifications
cross-dashboard-cli pomo task 'deploy hotfix'
cross-dashboard-cli pomo task -u 'exact-task-uid'
cross-dashboard-cli pomo issue 'owner/repo#42' --minutes 50
cross-dashboard-cli pomo fuzzel
cross-dashboard-cli pomo status
cross-dashboard-cli pomo pause
cross-dashboard-cli pomo resume
cross-dashboard-cli pomo stop
```

Title-based Pomodoro lookup is interactive: it ranks exact, substring, and fuzzy matches and asks
you to choose from a list showing calendar/due-date context. Use `task -u UID` in scripts or when
you already know the exact task UID.

The background service owns one global Pomodoro shared by the GTK UI, CLI, notifications, and
Waybar. Starting another timer while one is active is rejected; interrupting the terminal display
does not stop the background timer. The GTK Pomodoro popup can link a new session to any cached
incomplete task or open issue, and keeps pause/resume, stop, and skip controls available while it runs.

### Waybar module

The package installs a ready-to-merge module at
`/usr/share/cross-dashboard/waybar/config.jsonc` and its pill styling at
`/usr/share/cross-dashboard/waybar/style.css`. Add `custom/cross-dashboard` to the desired Waybar
module list, merge the module object into your config, and import or copy the CSS rules. The module
uses Nerd Font glyphs and behaves as follows:

- Active Pomodoro: live countdown and target.
- Left click opens Fuzzel while idle, or pauses/resumes an active Pomodoro.
- Otherwise: current event, overdue task, or the nearest upcoming event/task.
- Middle click opens Cross-Dashboard; right click asks the service to sync.
- No relevant item: the module hides itself.

Sync and calendar reminder delivery are owned by a persistent systemd user service, not the GTK
frontend or cron. Enable it once after a native install:

```bash
systemctl --user daemon-reload # needed after a direct `meson install`; distro packages do this
systemctl --user enable --now crossdashboard.service
systemctl --user status crossdashboard.service
journalctl --user -u crossdashboard.service
```

The service uses the sync interval configured in the app, keeps precise event reminder timers alive,
and exposes a user-session D-Bus endpoint for force refreshes from the GUI and CLI.

Shared under the MIT license. (If AI code is licensable:))
