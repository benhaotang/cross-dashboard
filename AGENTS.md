# Cross-Dashboard — AI Agent Instructions

## Project Overview

Cross-Dashboard is a unified personal productivity dashboard integrating:
- **CalDAV Events** — Calendar sync and event management
- **CalDAV Tasks (VTODO)** — Task management with subtasks and quick input
- **CalDAV Notes (VJOURNAL)** — Note-taking with CalDAV backend
- **Gitea Issues** — Issue tracking across Gitea repositories
- **Memos Capture** — Quick-capture notes via a self-hosted [Memos](https://usememos.com) server, displayed as the **Capture** screen

### Platform Strategy

| Platform | Status | Directory | Stack |
|---|---|---|---|
| **Android** | ✅ Complete | `native-android/` | Kotlin + Jetpack Compose |
| **macOS** | ✅ Complete | `native-macos/` | Swift 6.2 + SwiftUI |
| **Linux** | Planned | `native-linux/` (TBD) | Compose Multiplatform or GTK |
| React Native / Expo | Legacy — phased out | `src/`, `modules/` | TypeScript + Expo SDK 54 |

The RN/Expo codebase (`src/`) is frozen. Do not add new features to the RN code.

---

## Native Android (`native-android/`)

**Target**: Android 16 — `minSdk 36`, `targetSdk 36`, `compileSdk 36`  
**Language**: Kotlin 2.1 (100%)  
**UI**: Jetpack Compose with Material 3 "Expressive"

### Directory Structure

```
native-android/
├── app/
│   ├── build.gradle.kts
│   └── src/main/
│       ├── AndroidManifest.xml
│       └── kotlin/com/crossdashboard/app/
│           ├── MainActivity.kt
│           ├── CrossDashboardApp.kt          # @HiltAndroidApp, notification channels
│           ├── data/
│           │   ├── db/                       # Room: AppDatabase, entities, DAOs
│           │   ├── network/                  # CalDavClient, GiteaClient, MemosClient, NextcloudSsoHelper, NextcloudLoginFlow
│           │   ├── parser/                   # ICalParser, TaskInputParser
│           │   ├── repository/               # EventRepository, TaskRepository, NoteRepository, IssueRepository, MemoRepository, StatsRepository
│           │   ├── prefs/                    # SecureStore (Tink + Keystore), AppPreferences (DataStore)
│           │   └── di/                       # Hilt modules
│           ├── domain/
│           │   └── model/                    # Models.kt — all domain data classes
│           ├── ui/
│           │   ├── theme/                    # Theme.kt, Type.kt, Shape.kt
│           │   ├── navigation/               # AppNavigation.kt, Destination.kt
│           │   ├── adaptive/                 # FoldableUtils.kt
│           │   ├── screen/                   # dashboard/, events/, tasks/, notes/, issues/, inbox/, views/, memos/, settings/
│           │   ├── component/                # PropertySheet.kt, MarkdownText.kt, PomodoroBar.kt, PomodoroModal.kt, BiometricLockScreen.kt, CalendarColorResolver.kt, ...
│           │   ├── viewmodel/                # AppViewModel.kt, NavigationViewModel.kt
│           │   └── CrossDashboardRoot.kt
│           ├── service/
│           │   └── PomodoroForegroundService.kt
│           ├── worker/
│           │   └── SyncWorker.kt
│           ├── alarm/
│           │   ├── EventAlarmScheduler.kt
│           │   └── EventAlarmReceiver.kt
│           ├── receiver/
│           │   ├── BootReceiver.kt
│           │   └── AlarmPermissionReceiver.kt
│           └── widget/
│               ├── DashboardWidget.kt        # GlanceAppWidget
│               ├── DashboardWidgetReceiver.kt
│               └── WidgetStateDefinition.kt
├── gradle/
│   └── libs.versions.toml
├── build.gradle.kts
└── settings.gradle.kts
```

### Tech Stack

| Category | Library | Version |
|---|---|---|
| Language / coroutines | Kotlin 2.1, kotlinx-coroutines | 2.1.x / 1.10.x |
| UI | Jetpack Compose BOM | 2026.03.01 (Compose 1.10.x) |
| Material | material3 + material3-adaptive | 1.4.0 / 1.2.0 |
| Navigation | androidx.navigation3 (nav3) | 1.0.x |
| Architecture | ViewModel + Lifecycle | 2.9.x |
| DI | Hilt | 2.56.x |
| Network | OkHttp 5 | 5.1.x |
| Serialization | kotlinx.serialization-json | 1.8.x |
| Local DB | Room | 2.7.x |
| Background | WorkManager | 2.11.x |
| Widget | Glance + Glance Material3 | 1.1.x |
| Security | Tink (Google) | 1.16.x |
| Biometric | androidx.biometric | 1.4.x |
| DataStore | Preferences DataStore | 1.1.x |
| Image loading | Coil 3 (`coil-compose`) | 3.2.x |
| Nextcloud SSO | Android-SingleSignOn (JitPack) | 1.3.4 |
| Markdown / display LaTeX | multiplatform-markdown-renderer (mikepenz) + KotlinTeX | 0.39.2 / 0.3.3 |

### Adaptive UI Tiers

| Window | Width | Navigation | Layout |
|---|---|---|---|
| Phone | Compact (<600dp) | `NavigationBar` (bottom) | Single pane |
| Tablet / unfolded foldable | Medium (600–840dp) | `NavigationRail` | List + Detail side-by-side |
| Landscape tablet | Expanded (>840dp) | `NavigationDrawer` (persistent) | Sidebar + content |

`NavigationSuiteScaffold` auto-switches based on `WindowSizeClass`. List-detail screens use `NavigableListDetailPaneScaffold` on Medium/Expanded.

**Phone bottom-bar overflow:** When more than 6 nav items are visible on compact (phone) width, `AppNavigation.kt` shows the first 5 items plus a 6th "More" button. Tapping "More" opens a `DropdownMenu` listing all remaining destinations. This is handled entirely in `AppNavigation.kt` — no changes to `NavigationSuiteScaffold` itself are needed.

---

## Native macOS (`native-macos/`)

**Target**: macOS 26 "Tahoe" — `minimumDeploymentTarget: .macOS(.v26)` (bump `project.yml` to `"26.0"` when Xcode 26 SDK ships; currently `"15.0"` as placeholder)  
**Language**: Swift 6.2, strict concurrency (`SWIFT_STRICT_CONCURRENCY = complete`)  
**UI**: SwiftUI (pure — no AppKit wrappers except `NSStatusItem` for Pomodoro menu bar)

### Directory Structure

```
native-macos/
├── CrossDashboard.xcodeproj        # generated by xcodegen
├── project.yml                     # xcodegen spec
├── CrossDashboard/
│   ├── CrossDashboardApp.swift     # @main, WindowGroup + Settings scene, AppContainer init, handleDeepLink
│   ├── AppContainer.swift          # all singletons; EnvironmentKey
│   ├── App/
│   │   ├── AppDelegate.swift       # NSApplicationDelegate: registers Services provider; captureToMemos handler + ServiceCapturePanel
│   │   ├── AppViewModel.swift      # @Observable AppViewModel; cross-screen triggers (newTask, capture)
│   │   └── ContentView.swift       # NavigationSplitView 3-column root
│   ├── Data/
│   │   ├── Persistence/            # PersistenceController (SwiftData ModelContainer + @Model classes)
│   │   ├── Network/                # CalDavClient, GiteaClient, MemosClient, NextcloudLoginFlow
│   │   ├── Parser/                 # ICalParser, TaskInputParser
│   │   ├── Repository/             # EventRepository, TaskRepository, NoteRepository, IssueRepository, MemoRepository, StatsRepository
│   │   └── Prefs/                  # KeychainStore (SecItem* wrapper), AppPreferences (@Observable UserDefaults)
│   ├── Domain/
│   │   └── Models.swift            # All domain structs/enums — Codable; mirrors Models.kt 1:1
│   ├── UI/
│   │   ├── App/                    # ContentView (NavigationSplitView), AppViewModel
│   │   ├── Screens/                # Dashboard/, Events/, Tasks/, Notes/, Issues/, Inbox/, Views/, Memos/, Settings/, BiometricLockView.swift
│   │   └── Components/             # PropertyDetailShell, ReadField, ReadMarkdownField, CalendarColorDot, StatusBadge, PriorityChip, TagChip, PomodoroBar
│   ├── Background/                 # SyncScheduler (NSBackgroundActivityScheduler), NotificationScheduler (UNUserNotificationCenter)
│   └── Pomodoro/                   # PomodoroViewModel (@Observable singleton), PomodoroStatusItem (NSStatusItem)
└── DashboardWidgetExtension/       # WidgetKit extension; shares SwiftData store via App Group
```

### Tech Stack

| Category | Technology |
|---|---|
| Language | Swift 6.2, strict concurrency |
| UI | SwiftUI (macOS 26) |
| Persistence | SwiftData (`@Model`, `ModelContainer`, `ModelContext`) |
| Network | URLSession — arbitrary HTTP methods (`PROPFIND`, `REPORT`) |
| Serialization | Codable (Gitea DTOs); hand-written iCal parser |
| Credentials | Keychain Services (`SecItem*`); `KeychainStore` mirrors `SecureStore.kt` API |
| Non-sensitive prefs | `UserDefaults` / `@AppStorage` via `AppPreferences` |
| Background sync | `NSBackgroundActivityScheduler` + in-app `Timer` |
| Notifications | `UNUserNotificationCenter` + `UNCalendarNotificationTrigger` |
| Pomodoro | `NSStatusItem` menu bar countdown + `NSPopover` |
| Widget | WidgetKit extension (shared App Group SwiftData store) |
| Biometric | `LocalAuthentication` (`LAContext`) — Touch ID + SHA-256 PIN in Keychain |
| Markdown / display LaTeX | `swift-markdown-ui` + `SwiftMath` (SPM) |
| DI | `AppContainer` singleton + `@Environment` |

### Navigation

Three-column `NavigationSplitView`: sidebar (screen list) → content (list view) → detail (item detail). Replaces Android's `NavigationSuiteScaffold` + `NavigableListDetailPaneScaffold`. Deep link via `crossdashboard://` URL scheme (`onOpenURL`).

**Deep links handled:**
- `crossdashboard://tasks?action=add` → opens Tasks screen, triggers quick-input focus
- `crossdashboard://capture?text=<percent-encoded>` → navigates to Capture screen, opens compose sheet pre-filled with text (useful for browser bookmarklets, Alfred/Raycast actions, etc.)

**Single-window URL routing:** `WindowGroup` uses `.handlesExternalEvents(matching: Set(["*"]))` at scene level and `.handlesExternalEvents(preferring: Set(["crossdashboard"]), allowing: Set(["crossdashboard"]))` on `ContentView` so that incoming `crossdashboard://` URLs are routed to the existing window instead of opening a new one.

### Key Platform Differences from Android

| Android | macOS equivalent |
|---|---|
| Android Keystore + Tink AES-GCM | Keychain Services `kSecClassGenericPassword` |
| Nextcloud SSO (3 auth options) | Login Flow v2 + Manual only |
| WorkManager | `NSBackgroundActivityScheduler` + in-app `Timer` |
| `AlarmManager` exact alarms | `UNCalendarNotificationTrigger` |
| `PomodoroForegroundService` live chip | `NSStatusItem` menu bar countdown |
| `NavigationSuiteScaffold` (bottom/rail/drawer) | `NavigationSplitView` three-column |
| Glance widget | WidgetKit extension |
| `BiometricPrompt` + PIN | `LAContext` + hashed PIN in Keychain |

---

## Key Architecture Decisions

### Data Layer
- **Room is the source of truth.** UI observes `Flow<List<T>>` from DAOs and renders immediately from cache.
- `SyncWorker` runs in the background (WorkManager) and does `clearAll() + upsert(freshData)` into Room.
- No SharedPreferences credential duplication — `SecureStore` is available directly in `@HiltWorker` via Hilt injection.

### Credentials & Settings
- **`SecureStore`** (`data/prefs/SecureStore.kt`): AES-256-GCM key in Android Keystore (hardware-backed on API 36). Encrypts all credential values before writing to a plain `SharedPreferences` file excluded from Auto Backup.
- **`AppPreferences`** (`data/prefs/AppPreferences.kt`): Preferences DataStore for non-sensitive settings (theme, visible screens, kanban columns, pomodoro, notifications, sync interval, biometric lock).
- `CredentialKey` constants mirror the RN `keyring.ts` keys for conceptual continuity.

### Network
- `CalDavClient` uses raw OkHttp 5 — no Retrofit. CalDAV uses non-standard HTTP methods (`PROPFIND`, `REPORT`, `MKCALENDAR`) that require direct `Request.Builder` usage.
- `ICalParser` is a hand-written RFC 5545 line-by-line parser (no third-party iCal library) supporting VEVENT/VTODO/VJOURNAL read + serialization.
- `GiteaClient` uses kotlinx.serialization DTOs with `toDomain()` mappers. Multipart uploads (OkHttp `MultipartBody`) are used for issue and comment attachments.
- `MemosClient` talks to the Memos REST API v1 (`/api/v1/memos`). Auth is `Authorization: Bearer <token>`. Uses OkHttp (Android) / URLSession (macOS). All DTO fields that can be absent in proto3 JSON must be `nullable` (Android) or `Optional` (Swift) with defaults — never non-optional for fields the server omits.

### Nextcloud Auth (three options)
1. **Nextcloud SSO** (`NextcloudSsoHelper`) — AIDL IPC via `Android-SingleSignOn`; preferred when NC app is installed (e.g. LineageOS + F-Droid).
2. **Login Flow v2** (`NextcloudLoginFlow`) — browser-based, no NC app required; polls every 2s for up to 5 min.
3. **Manual CalDAV** — server + username + password.

### Navigation (Nav3)
Type-safe `sealed class Destination : Parcelable` with `@Parcelize`. `AppNavigation.kt` uses `NavDisplay` + `NavigationSuiteScaffold`. Deep link `crossdashboard://tasks?action=add` handled in `MainActivity.onNewIntent` → sets `autoFocusQuickInput = true` on `TasksViewModel`.

**Screen ordering:** `AppPreferences.visibleScreens` (Android DataStore) / `AppPreferences.visibleScreens` (macOS UserDefaults) stores the **ordered** list of visible screen names as a comma-separated string. Screens are shown in stored order — not in `ALL_SCREENS` / `Screen.allCases` order. In `AppNavigation.kt`, `navItems` is built by mapping `visibleScreens` (in order) to `NavItem` entries. In `AppViewModel.swift`, `visibleScreens` maps `preferences.visibleScreens` through `Screen(rawValue:)`. Settings lets users reorder screens: drag-to-reorder `List` (macOS) or up/down `IconButton` rows (Android).

**Phone overflow:** On compact width, if more than 6 destinations are visible, `AppNavigation.kt` caps the bottom bar at 5 items + a "More" `DropdownMenu` for the remaining items.

### Pomodoro Timer
Uses Android 16 **Live Update** (promoted ongoing notification) API:
- `PomodoroForegroundService` calls `setRequestPromotedOngoing(true)` + `setChronometerCountDown(true)` → countdown chip in the status bar.
- `PomodoroViewModel` (`@Singleton` Hilt) owns all timer state and is shared across all composables.
- `PomodoroBar` composable (bottom-end popup) visible when timer active and modal hidden.
- Session logging written to task description via `CalDavClient.updateTask()`.

### Glance Widget
`DashboardWidget` (GlanceAppWidget) replaces the old `AppWidgetProvider` + RemoteViews approach. Widget state is a `DashboardWidgetState` backed by a DataStore serializer. `SyncWorker` calls `GlanceAppWidgetManager.updateIf<DashboardWidget>()` after each sync. `SizeMode.Responsive` with three size buckets (small/medium/large) controls 1–3 visible rows per section.

### Exact Alarms
`USE_EXACT_ALARM` permission declared (auto-granted, non-revocable — appropriate for a calendar app). Stable alarm IDs from `abs(uid.hashCode()) % 100_000`. `EventAlarmScheduler.rescheduleAll()` called after every sync and on boot.

### Markdown + Display LaTeX Rendering
Read-only detail views render descriptions and note bodies as GitHub-Flavoured Markdown. Android uses `multiplatform-markdown-renderer` (Mike Penz, v0.39.2) plus `KotlinTeX` (v0.3.3) for display equations. macOS uses `swift-markdown-ui` plus `SwiftMath`.

**Key components in `ui/component/`:**
- `MarkdownText(content, modifier)` — low-level composable. Colors are mapped to `MaterialTheme.colorScheme` tokens; typography to the M3 type scale. Images load via the app's existing Coil 3 instance (`Coil3ImageTransformerImpl`).
- `LatexContentParser.kt` — splits markdown from display-math blocks before rendering.
- `ReadMarkdownField(label, value, modifier)` — drop-in replacement for `ReadField` when the value may contain markdown. Shows the same small-caps label but renders the body through `MarkdownText`.

**LaTeX rules:**
- Only `$$...$$` is rendered as LaTeX, and it is styled as centered display math with extra vertical spacing.
- Inline `$...$` is not rendered as math. It stays in the markdown text and is escaped before markdown parsing so LaTeX underscores and similar characters do not accidentally trigger emphasis or other markdown formatting.

**Where markdown is rendered (read-only paths only):**

| Surface | Field | Component used |
|---|---|---|
| Task detail (`TaskReadView`) | Description | `ReadMarkdownField` |
| Event detail (`EventPropertySheet` + `EventDetailContent`) | Description | `ReadMarkdownField` |
| Note detail (`NoteReadView`) | Body | `MarkdownText` directly |
| Issue detail (`IssueReadContent`) | Description (body) | `MarkdownText` directly |
| Issue detail (`CommentItem`) | Comment body | `MarkdownText` directly |
| Capture memo detail (`MemoDetailView` / `MemoPropertySheet`) | Memo body | `Markdown` (swift-markdown-ui) / `MarkdownText` directly |

**Rule:** Use `ReadMarkdownField` / `MarkdownText` only in read-only branches. Edit forms always use plain `OutlinedTextField` / `TextEditor`.

### Issues (Gitea) — Feature Details

All Gitea issue operations go through `GiteaClient` → `IssueRepository` → `IssuesViewModel`.

**Create issue**
- FAB (`+`) on `IssuesScreen` calls `viewModel.showCreateSheet()`.
- `CreateIssueSheet` (bottom sheet) provides: repository dropdown (from `GITEA_REPOS` credential), title, description, and optional file attachments.
- Flow: `POST /repos/{owner}/{repo}/issues` → save to Room → upload each `PendingAttachment` via `POST /repos/{owner}/{repo}/issues/{index}/assets` (multipart/form-data).

**Attachments — creating / uploading**
- `PendingAttachment(fileName, mimeType, bytes)` is a UI-layer data class defined in `IssuesViewModel.kt`.
- Files are read from a `Uri` via `ContentResolver.openInputStream()` inside a `Dispatchers.IO` coroutine launched from `rememberCoroutineScope()` in the composable.
- Issue attachments: uploaded after `createIssue()` succeeds.
- Comment attachments: uploaded after `addComment()` succeeds.
- `GiteaClient.uploadIssueAttachment()` / `uploadCommentAttachment()` both POST `multipart/form-data` with a single field named `attachment`.

**Attachments — fetching / displaying**
- `GiteaAttachment(id, name, downloadUrl, size, uuid)` is defined in `domain/model/Models.kt`.
- `IssuesUiState` holds `issueAttachments: Map<Long, List<GiteaAttachment>>` (keyed by issue id) and `commentAttachments: Map<Long, List<GiteaAttachment>>` (keyed by comment id).
- Loaded in `loadComments()` using `async`/`await` in parallel: issue attachments (`GET /repos/{owner}/{repo}/issues/{index}/assets`) and all per-comment attachments (`GET /repos/{owner}/{repo}/issues/comments/{id}/assets`) are fetched alongside comments.
- `AttachmentLink` composable (in `IssuePropertySheet.kt`) renders each attachment as a tappable row with a paperclip icon, filename, and file size. Tapping fires `Intent.ACTION_VIEW` with the download URL to open in the system browser.
- Issue attachments appear as a dedicated "Attachments" section below the issue description in `IssueReadContent`.
- Comment attachments appear below each comment's markdown body inside `CommentItem`.

**Comment input with attachments**
- Paperclip icon button beside the send button opens the system file picker (`ActivityResultContracts.GetContent("*/*")`).
- Selected files appear as `InputChip` removable chips above the text field.
- On send, `onAddComment(body, pendingAttachments)` is called; the ViewModel posts the comment then uploads each attachment.

### Capture (Memos) — Feature Details

The **Capture** screen (`Destination.Memos` / `Screen.memos`) integrates with a self-hosted [Memos](https://usememos.com) server. Credentials are stored in `SecureStore` / `KeychainStore` under `CredentialKey.MEMOS_HOST` and `CredentialKey.MEMOS_TOKEN`.

**API client (`MemosClient`)**
- Base URL read from `CredentialKey.MEMOS_HOST` (trailing slash stripped).
- Every request carries `Authorization: Bearer <MEMOS_TOKEN>`.
- Uses Memos REST API v1: `GET /api/v1/memos`, `POST /api/v1/memos`, `PATCH /api/v1/memos/{name}`, `DELETE /api/v1/memos/{name}`, comment/attachment/share/relation endpoints.
- **Critical DTO rule (macOS Swift):** The Memos server uses proto3 JSON, which omits fields that are the default value (false, empty string, empty array). All `MemoDtoFull` sub-struct fields (`MemoPropertyDto`, `AttachmentDto`, etc.) must be `Optional` (`Bool?`, `String?`, `[T]?`) so a missing key decodes as `nil` rather than throwing `keyNotFound`. Never declare non-optional struct fields for Memos DTOs unless the API guarantees the key is always present.
- **Attachment file URL:** `GET {host}/file/{att.name}/{att.filename}` — e.g. `/file/attachments/gUeHhaXss87/photo.png`. Do **not** strip the `attachments/` prefix from `att.name`.

**Persistence**
- Android: `MemoEntity` in Room (`data/db/entity/Entities.kt`), `MemosDao` in `data/db/dao/Daos.kt`, mapped via `Mappers.kt`.
- macOS: `MemosModel` (`@Model`) in `PersistenceController.swift`; `init(from:)` and `toDomain()` mappers inline on the model class.

**Repository (`MemoRepository`)**
- `syncMemos()`: fetches all NORMAL then ARCHIVED memos with pagination (follows `nextPageToken`), clears DB, upserts fresh data, then calls `loadFromDB()`.
- `allMemos` (Android `Flow<List<MemosMemo>>` / macOS `@Published [MemosMemo]`): source of truth for the UI.
- CRUD: `createMemo`, `updateMemo`, `deleteMemo` (soft archive or force delete), `archiveMemo`, `restoreMemo`.
- Comments: `loadComments`, `createComment`.
- Attachments: `createAttachment`, `deleteAttachments`.
- Share: `createShare` → returns `{host}/s/{token}` public share URL.

**UI**
- `MemosScreen` (Android) / `MemosView` (macOS): list of memos with state filter chips (Normal / Archived), tag filter chips, search, swipe-to-delete/archive actions.
- `MemoPropertySheet` (Android) / `MemoDetailView` (macOS): full memo detail with markdown body, attachments, comment thread, and action toolbar.
- **Attachment image preview:** JPEG/PNG/GIF/WEBP attachments show a 160dp/pt inline thumbnail loaded with auth headers via `MemoAuthImage` (Android, `HttpURLConnection`) / `MemoAuthImageView` (macOS, `URLSession`). Tapping opens the file in the system browser.
- **Toolbar actions** (contextual — only shown when relevant):
  - **Extract Tasks** — parses `- [ ] …` lines via `TaskInputParser`, shows confirmation sheet.
  - **Create Event** — seeds an event from detected dates/keywords in memo content.
  - **Comment on Issue** — picks a Gitea repo (from `GITEA_REPOS` credential, decoded from JSON array) then picks an open issue from the locally-synced `IssueRepository`; no free-form issue number input.
  - **Open URL** — shown when `NSDataDetector` / URL regex finds a URL in content (regardless of `property.hasLink` flag).
  - **Copy Link** — copies `{memosHost}/{memo.name}` to the clipboard; shows a 2-second toast. Does **not** call the share API.
- **Visibility badge** in `MemoListRow` / list rows: `lock.fill` (private, secondary), `person.2.fill` (protected, orange), `globe` (public, tint).
- **Share Extension** (Android `MemosShareExtension` / macOS `MemosShareExtension`): handles `ACTION_SEND`/share-services intent to create a new memo pre-filled with shared text/URL. Android accepts `text/plain` and `*/*` (any file type) for both `ACTION_SEND` and `ACTION_SEND_MULTIPLE`.
- **macOS Services menu (`NSServices`)**: registered in the main app's `Info.plist` via `NSServices` → `captureToMemos` handler in `App/AppDelegate.swift`. **Caveat**: on macOS 26 Tahoe, the Services menu entry is only shown for apps signed with a Developer ID or App Store certificate. Debug builds signed with "Apple Development" are rejected by `spctl` and their services are filtered out by the OS. `sudo spctl --add` is no longer supported in macOS 26. The Services registration code is kept as-is for release builds; use the `crossdashboard://capture?text=` URL scheme as the equivalent for development.

---

## Domain Models (Kotlin)

All domain types live in `domain/model/Models.kt`. They are direct ports of the TypeScript interfaces from the legacy `src/types/index.ts`:

```kotlin
data class CalDavTask(
    val uid: String,
    val summary: String,
    val description: String? = null,
    val status: TaskStatus = TaskStatus.NEEDS_ACTION,
    val priority: Int = 0,        // 0=none, 1-4=high, 5=med, 6-9=low
    val percentComplete: Int = 0,
    val due: Instant? = null,
    val dtstart: Instant? = null,
    val completed: Instant? = null,
    val created: Instant,
    val lastModified: Instant,
    val categories: List<String> = emptyList(),
    val location: String? = null,
    val parentUid: String? = null,
    val calendarHref: String? = null,
)

enum class TaskStatus { NEEDS_ACTION, IN_PROCESS, COMPLETED, CANCELLED }
```

Similar for `CalendarEvent`, `Note`, `GiteaIssue`, `GiteaComment`, `CalDavCalendar`, `GiteaLabel`, `GiteaMilestone`, `InboxItem`, `PomodoroSettings`, `PomodoroPhase`, `PomodoroState`, `TaskDefaults`, `ParsedTask`, `DailyStats`, `AppSettings`.

**Memos models:**

```kotlin
data class MemosMemo(
    val name: String,                     // "memos/{id}"
    val state: MemoState,
    val content: String,
    val visibility: MemoVisibility,
    val tags: List<String>,
    val pinned: Boolean,
    val attachments: List<MemosAttachment>,
    val property: MemoProperty,
    val snippet: String,
    val createTime: Instant,
    val displayTime: Instant,
    val updateTime: Instant,
)
enum class MemoState    { NORMAL, ARCHIVED }
enum class MemoVisibility { PRIVATE, PROTECTED_, PUBLIC }   // PRIVATE = "PRIVATE" raw value
data class MemoProperty(val hasLink: Boolean, val hasTaskList: Boolean,
                        val hasIncompleteTasks: Boolean, val title: String)
data class MemosAttachment(
    val name: String,       // "attachments/{id}"
    val filename: String,
    val externalLink: String,
    val type: String,       // MIME type
    val size: Long,
    val memo: String,       // parent memo name
)
```

`ALL_SCREENS` constant: `listOf("Dashboard", "Inbox", "Events", "Tasks", "Notes", "Issues", "Views", "Capture")` — "Capture" is the display name for the Memos screen.

---

## Task Quick Input Syntax

`TaskInputParser.kt` is a port of the legacy `taskParser.ts`:

```
!! meet friends #social tonight    → priority=medium, tag=social, due=today 21:00
!!! deploy hotfix tomorrow morning → priority=high, due=tomorrow 08:00
buy milk #errands                  → no priority, tag=errands, no due
call mom monday                    → due=next Monday 10:00
```

Priority: `!` → low(9), `!!` → medium(5), `!!!` → high(1). Tags: `#word` → `categories`. Time-of-day hours are configurable via `TaskDefaults` in Settings.

---

## Screens

| Screen | ViewModel | Notes |
|---|---|---|
| Dashboard | `DashboardViewModel` | Stats card (7-day), upcoming events, tasks due soon, open issues |
| Events | `EventsViewModel` | Day/week/month filter, calendar color dots, adaptive list-detail |
| Tasks | `TasksViewModel` | Quick input bar, nested subtask tree, filter, kanban chips, Pomodoro |
| Notes | `NotesViewModel` | Grid/list view, search, VJOURNAL CRUD |
| Issues | `IssuesViewModel` | State filter, create issue (FAB), comments, attachments (issue + comment), open/close toggle, Pomodoro |
| Views | `ViewsViewModel` | Kanban + Covey Four Quadrants, assign modal, sync back to CalDAV/Gitea |
| Inbox | `InboxViewModel` | Unified list; total estimated time from durations and `#Xm`/`#Xh` tags |
| **Capture** | `MemosViewModel` | Memos integration: list/detail, create, archive/delete, comments, attachments (image preview), action sheets (extract tasks, create event, comment on issue, open URL, copy link), share extension |
| Settings | `SettingsViewModel` | CalDAV (SSO/LoginFlow/Manual), calendars, Gitea, Memos, theme, nav reorder, Pomodoro, alarms, widget sync, biometric |

Property sheets: `ModalBottomSheet` on phone; inline detail pane (`NavigableListDetailPaneScaffold`) on tablet/Expanded.

Screen display name is **"Capture"**; internal `Destination` object remains `Destination.Memos` and macOS enum case remains `Screen.memos` (rawValue `"Capture"`). Do not create a new destination — reuse `Destination.Memos`.

---

## Coding Conventions (Android)

- **Kotlin only** — no Java. Strict null safety; avoid `!!` except where unavoidable.
- **Compose functions**: PascalCase, `@Composable`, no side effects in composable bodies.
- **ViewModels**: `@HiltViewModel`, expose `StateFlow<UiState>`, use `viewModelScope`.
- **Repositories**: inject into ViewModels; never call network/DB from composables directly.
- **Coroutines**: all suspend functions on `Dispatchers.IO` in the data layer; `viewModelScope.launch` in ViewModels.
- **Error handling**: `Result<T>` or sealed `UiState` with error state; never crash silently.
- **Accessibility**: every interactive element needs a `contentDescription`; use `semantics(mergeDescendants = true)` on compound rows.
- **Dark/light**: use `MaterialTheme.colorScheme` tokens only — no hardcoded colors.

### Naming
- Files/classes: PascalCase (`TasksViewModel.kt`)
- Composables: PascalCase (`TaskRow`, `QuickInputBar`)
- ViewModels: `<Screen>ViewModel`
- Repositories: `<Entity>Repository`
- Workers/Receivers/Services: descriptive suffix (`SyncWorker`, `BootReceiver`)

---

## Security Guidelines

- **NEVER** hardcode credentials or API tokens.
- All sensitive data goes through `SecureStore` (Tink + Android Keystore). Never write credentials to DataStore or plain SharedPreferences.
- `SecureStore`'s backing file (`secure_credentials.xml`) is excluded from Auto Backup and Device Transfer via `data_extraction_rules.xml` and `backup_rules.xml`.
- Validate all API responses before processing; log errors without sensitive fields.

---

## Implementation Status

### Native Android — All phases complete ✅

**Phase 1** — Data layer: Room DB (5 entities + DAOs), `SecureStore`, `AppPreferences`, `CalDavClient` + `ICalParser`, `GiteaClient`, `TaskInputParser`, all Repositories, Hilt DI modules.

**Phase 2** — Core UI: `MainActivity` (edge-to-edge, splash), Nav3 type-safe routes, `NavigationSuiteScaffold` adaptive scaffold, Material 3 dynamic color theme, `DashboardScreen`, `TasksScreen` with quick input + nested subtree, `PropertySheet` component.

**Phase 3** — Remaining screens: `EventsScreen`, `NotesScreen`, `IssuesScreen` (comments), `InboxScreen` (time totals), `ViewsScreen` (Kanban + Covey + assign modal), `SettingsScreen` (all sections including Nextcloud SSO + Login Flow v2).

**Phase 4** — Background: `SyncWorker` (WorkManager), `EventAlarmScheduler` + `EventAlarmReceiver`, `BootReceiver` + `AlarmPermissionReceiver`, Glance widget (`DashboardWidget`) with `SizeMode.Responsive` adaptive rows.

**Phase 5** — Live notifications: Notification channels, `PomodoroForegroundService` with `Notification.ProgressStyle` live update (API 36), `PomodoroViewModel` + `PomodoroBar` + `PomodoroModal`, session logging to task description.

**Phase 6** — Polish: `NavigableListDetailPaneScaffold` on all list screens (events/tasks/notes/issues), `FoldableUtils.kt` hinge avoidance, predictive back in `PropertySheet`, shared element transitions on Tasks, full TalkBack accessibility audit, dark mode refinement, ProGuard rules.

**Phase 7** — Issues enhancements: `CreateIssueSheet` (create issue with title, body, repo selector, file attachments via SAF), `PendingAttachment` UI data class, `GiteaAttachment` domain model, `AttachmentLink` composable, `GiteaClient` multipart upload/fetch methods for issue and comment assets, attachment display in `IssueReadContent` (issue-level section) and `CommentItem` (per-comment), attachment support in comment input bar.

**Phase 8** — Capture (Memos) screen: `MemoEntity` + `MemosDao` in Room, `MemosClient` (OkHttp, Bearer auth, Memos API v1), `MemoRepository`, `MemosViewModel` + `MemosUiState`, `MemosScreen` (list with filters/search/swipe), `MemoPropertySheet` (detail, markdown body, action sheets), `CommentOnIssueSheet` (repo picker + issue picker from local Room cache), `CreateMemoSheet`, `ExtractTasksConfirmSheet`, `CreateEventFromMemoSheet`, `MemoCommentItem`, `MemoAuthImage` (authenticated image thumbnail via `HttpURLConnection`), `MemosShareExtension` (`ACTION_SEND`), Settings integration (`MEMOS_HOST` / `MEMOS_TOKEN`), visibility badge in list rows.

**Phase 9** — Navigation reorder + overflow: `visibleScreens` preference now stores ordered list; Settings **Navigation** section shows visible screens with ↑/↓ reorder buttons and hidden screens with toggle-to-restore; phone bottom bar caps at 6 items with a "More" `DropdownMenu` overflow.

### Native macOS — All phases complete ✅

**Phase 1** — Data layer: SwiftData `PersistenceController` (5 `@Model` types), `KeychainStore`, `AppPreferences`, `CalDavClient` + `ICalParser`, `GiteaClient`, `TaskInputParser`, all Repositories, `AppContainer`.

**Phase 2** — Core UI: `CrossDashboardApp` (`@main`), `ContentView` (`NavigationSplitView` 3-col), `AppViewModel`, theme, `DashboardView`, `TasksView` + `QuickInputBar` + `TaskDetailView`, shared components (`PropertyDetailShell`, `ReadField`, `CalendarColorDot`, `StatusBadge`, `PriorityChip`, `TagChip`).

**Phase 3** — Remaining screens: `EventsView`, `NotesView`, `IssuesView` (comments + attachments via `NSOpenPanel`), `InboxView`, `ViewsView` (Kanban + Covey), `SettingsView` (all sections; Login Flow v2 + Manual only — no SSO), `ReadMarkdownField` / `ReadMarkdownView` via `swift-markdown-ui`, display LaTeX via `SwiftMath`.

**Phase 4** — Background: `SyncScheduler` (`NSBackgroundActivityScheduler`), `NotificationScheduler` (`UNCalendarNotificationTrigger`), on-launch rescheduling (mirrors `BootReceiver`), `UNUserNotificationCenterDelegate`.

**Phase 5** — Pomodoro: `PomodoroViewModel` (`@Observable` singleton, `Timer`-based), `PomodoroStatusItem` (`NSStatusItem` + `NSPopover`), `PomodoroBar` floating panel + `PomodoroModal`, session logging to task description.

**Phase 6** — Polish: `DashboardWidgetExtension` (WidgetKit, App Group shared SwiftData store, three `WidgetFamily` sizes), `BiometricLockView` (Touch ID + 6-digit PIN, SHA-256 via CryptoKit), toolbar + keyboard shortcuts (`Cmd+N`, `Cmd+R`, `Cmd+Delete`), VoiceOver accessibility audit, dark mode polish, notification tap navigation.

**Phase 7** — Capture (Memos) screen: `MemosModel` (`@Model` in SwiftData), `MemosClient` (URLSession, Bearer auth, Memos API v1), `MemoRepository`, `MemosViewModel` (`@Observable @MainActor`), `MemosView` (list with filter chips, search, swipe actions, visibility badges), `MemoDetailView` (markdown body via `swift-markdown-ui`, attachments with `MemoAuthImageView` authenticated thumbnail, comments, toolbar actions), `CommentOnIssueSheetMac` (repo + issue pickers), `CreateMemoSheetMac`, `ExtractTasksSheetMac`, `CreateEventFromMemoSheetMac`, `MemosShareExtension` (share-services extension), Settings integration.

**Phase 8** — Navigation reorder: `AppViewModel.visibleScreens` now preserves preference order; `SettingsViewModel` exposes `orderedVisibleScreens: [String]` + `toggleVisibleScreen` + `moveVisibleScreen`; Settings Appearance tab shows a drag-reorderable `List` (with `.onMove`) for visible screens and a hidden-screens restore section.

**Phase 9 (macOS)** — Share & URL improvements: `App/AppDelegate.swift` (`NSApplicationDelegate`, `NSServices` provider, `ServiceCapturePanel` floating panel); `crossdashboard://capture?text=` URL scheme (`AppViewModel.triggerCapture`, `MemosView` `.onAppear`+`.onChange` dual-handler, `CreateMemoSheetMac` `initialText` param); share extension filename fix (separate `public.file-url` load for original name + `loadFileRepresentation` for bytes, editable filename field in compose UI); `WindowGroup.handlesExternalEvents` single-window URL routing.

**Phase 9 (Android)** — Share & filename fixes: `AndroidManifest.xml` `ACTION_SEND` changed from `image/*` to `*/*` (all file types); `CreateMemoSheet.kt` and `MainActivity.parseShareIntent` fixed to use `OpenableColumns.DISPLAY_NAME` instead of `uri.lastPathSegment` for correct attachment filenames.

### Remaining / Next Steps
- Android: end-to-end smoke test (`./gradlew assembleDebug`) — fix any remaining compilation errors
- Android: shared element transitions for Events, Notes, Issues, Capture screens (lower priority)
- macOS: open `CrossDashboard.xcodeproj` in Xcode 16+, build, fix any Swift 6 strict concurrency warnings; bump `MACOSX_DEPLOYMENT_TARGET` to `26.0` when Xcode 26 SDK ships
- macOS: "Change PIN" flow in Settings → Security
- Linux native target (TBD)

---

## Development Commands

```bash
# Native Android
cd native-android
./gradlew assembleDebug          # build debug APK
./gradlew installDebug           # install on connected device/emulator
./gradlew lint                   # lint checks
./gradlew test                   # unit tests

# Native macOS
cd native-macos
xcodegen generate                # regenerate .xcodeproj after adding/removing Swift files
open CrossDashboard.xcodeproj    # build + run in Xcode

# Legacy RN (frozen — do not add features)
pnpm install
pnpm start
pnpm android
pnpm web
pnpm typecheck
```

---

## Version Bumping

When releasing a new version, update:
1. `native-android/app/build.gradle.kts` — `versionName` and `versionCode`
2. `native-android/app/src/main/kotlin/.../ui/screen/settings/SettingsScreen.kt` — version string in About section
3. `native-macos/CrossDashboard/Info.plist` (or `project.yml` `MARKETING_VERSION`) — `CFBundleShortVersionString`

The RN `package.json` / `app.json` version fields are legacy and no longer need to stay in sync.

---

## Agent Task Guidelines

### Adding a Feature (Android)
1. Define/update domain models in `domain/model/Models.kt` if new data structures are needed.
2. Add DB entity + DAO in `data/db/` if persistence is required; add `toDomain()` / `toEntity()` mappers in `Mappers.kt`.
3. Extend `CalDavClient`, `GiteaClient`, or `MemosClient` in `data/network/` for new API calls.
4. Update the relevant Repository with new methods.
5. Add ViewModel logic (`@HiltViewModel`), exposing `StateFlow<UiState>`.
6. Build Compose UI in the appropriate `ui/screen/` directory.
7. Wire new routes in `Destination.kt` + `AppNavigation.kt` if a new screen.

### Credentials
- Use `SecureStore.set/get/delete` (Android) or `KeychainStore.set/get/delete` (macOS) for all sensitive values. Never pass raw passwords/tokens beyond the point of initial entry.
- Memos credentials: `CredentialKey.MEMOS_HOST` (base URL, trailing slash stripped at read time) and `CredentialKey.MEMOS_TOKEN` (Bearer token).
- Gitea repos: `CredentialKey.GITEA_REPOS` is stored as a **JSON-encoded array** (`["owner/repo","owner/repo2"]`) on macOS (written by `SettingsViewModel.saveGitea()`). On Android it is stored as a **comma-separated string**. Always decode both formats when reading: try JSON array first, fall back to comma-split.

### UI (Android)
- Always support dark and light mode using `MaterialTheme.colorScheme` tokens.
- Use `NavigationSuiteScaffold` + `NavigableListDetailPaneScaffold` patterns for adaptive layouts.
- Every interactive composable must have a `contentDescription` for TalkBack.
- Wrap bottom sheets in `ModalBottomSheet` with `WindowInsets.ime` padding for keyboard avoidance.
- For description/body fields in **read-only** detail views, use `ReadMarkdownField` (labelled) or `MarkdownText` (unlabelled). `$$...$$` may be rendered as display math; inline `$...$` should remain literal text unless the rendering architecture is explicitly changed. Edit forms always use `OutlinedTextField`.

### Background Work (Android)
- Use `WorkManager` for all deferrable background tasks. Inject with `@HiltWorker`.
- After any sync, call `EventAlarmScheduler.rescheduleAll()` and `GlanceAppWidgetManager.updateIf<DashboardWidget>()`.

### Adding a Feature (macOS)
1. Define/update domain types in `Domain/Models.swift` if new data structures are needed.
2. Add a SwiftData `@Model` class in `Data/Persistence/` if persistence is required; add `init(from:)` + `toDomain()` mappers.
3. Extend `CalDavClient`, `GiteaClient`, or `MemosClient` in `Data/Network/` for new API calls.
4. Update the relevant Repository with new methods.
5. Add `@Observable @MainActor` ViewModel logic, exposing published state.
6. Build SwiftUI views in the appropriate `UI/Screens/` directory.
7. Wire new navigation cases in `ContentView.swift` and `AppViewModel.swift`.
8. Run `xcodegen generate` in `native-macos/` after adding/removing Swift files.

### UI (macOS)
- Use `Color(.label)`, `Color(.secondaryLabel)`, `Color(.windowBackgroundColor)` — no hardcoded hex or `.black`/`.white`.
- Navigation via `NavigationSplitView` three-column; detail shown inline (no sheets for primary detail).
- Every interactive element needs `accessibilityLabel` / `accessibilityValue` / `accessibilityHint` for VoiceOver.
- For description/body fields in **read-only** detail views, use `ReadMarkdownField` (labelled) or `ReadMarkdownView` (unlabelled) via `swift-markdown-ui`. `$$...$$` may be rendered as display math via `SwiftMath`; inline `$...$` should remain literal text unless the rendering architecture is explicitly changed. Edit forms always use `TextField` / `TextEditor`.
- Prefer toolbar `ToolbarItem` over FABs; add `keyboardShortcut` for common actions.

### Memos / Capture Screen Gotchas
- **Proto3 JSON nullability (macOS Swift):** The Memos server omits default-value fields from JSON responses. Any Swift DTO struct used with `JSONDecoder` must declare all fields as `Optional` (`Bool?`, `String?`, `[T]?`). A non-optional `var title: String = ""` in a synthesized `Decodable` struct will throw `keyNotFound` when `"title"` is absent — the default value is ignored by the synthesized `init(from:)`. Always use `var title: String? = nil` and apply `?? ""` in `toDomain()`.
- **Attachment file URL:** Construct as `{host}/file/{att.name}/{att.filename}` where `att.name` already includes the `attachments/` prefix (e.g. `attachments/gUeHhaXss87`). Do not strip or split the name.
- **GITEA_REPOS format:** macOS saves this as a JSON array; Android saves as comma-separated. `MemosViewModel.configuredRepos` (macOS) decodes JSON first; `MemosViewModel` init (Android) splits by comma. Both handle their respective native formats correctly.
- **Screen name:** The Memos feature is displayed to users as **"Capture"**. The internal `Destination.Memos` / `Screen.memos` objects have `rawValue = "Capture"`. `ALL_SCREENS` contains `"Capture"`. Do not use the string `"Memos"` in any user-visible label or in the `ALL_SCREENS` / `allScreens` constants.
- **macOS share extension filename:** `NSItemProvider.suggestedName` is unreliable (often `nil`) for Finder-originated shares. `loadFileRepresentation(forTypeIdentifier:)` copies the file to a macOS temp directory and names the copy after the UTI description (e.g. "rich text (RTF).rtf"), NOT the original filename. The correct approach: separately load `public.file-url` first via `loadItem(forTypeIdentifier:)` and use `url.lastPathComponent` as the authoritative filename; use `loadFileRepresentation` only for reading the file bytes. See `loadOriginalFilename(from:)` in `MemosShareExtension/ShareViewController.swift`.
- **Android attachment filename:** `uri.lastPathSegment` on a `content://` URI returns the internal document ID (e.g. `msf:162`), not the display name. Always query `OpenableColumns.DISPLAY_NAME` via `ContentResolver.query()` to get the real filename. See `CreateMemoSheet.kt` and `MainActivity.kt` for the correct pattern. `CreateIssueSheet.kt` and `IssuePropertySheet.kt` already use this pattern correctly.
- **Android share MIME filter:** `AndroidManifest.xml` declares `ACTION_SEND` with `mimeType="*/*"` (not `image/*`) so that the app appears in the share sheet for all file types (PDFs, archives, Office docs, etc.), not just images. `ACTION_SEND_MULTIPLE` also uses `*/*`.
- **macOS URL scheme capture — cold-start timing:** `AppViewModel.triggerCapture(text:)` may fire during app init before `MemosView` has rendered. SwiftUI's `.onChange` misses values set before the view appeared. `MemosView` handles this with both `.onAppear` (catches cold-start) and `.onChange` (catches warm URL invocations). Always use both modifiers for `captureInitialText`-style triggers.
