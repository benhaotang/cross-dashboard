# Cross Dashboard

This is an experimental client for self-hosting services like CalDAV, [Gitea](https://gitea.io), [Memos](https://usememos.com), [Nextcloud](https://nextcloud.com), etc.

90% coded by Cursor (Sonnet 4.6, GPT 5.4 and Kimi K2.5)

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
| **Linux** | Planned | `native-linux/` (TBD) | GTK |

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

Shared under the MIT license. (If AI code is licensable:))
