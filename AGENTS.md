# Cross-Dashboard — AI Agent Instructions

## Project Overview

Cross-Dashboard is a unified personal productivity dashboard integrating:
- **CalDAV Events** — Calendar sync and event management
- **CalDAV Tasks (VTODO)** — Task management with subtasks and quick input
- **CalDAV Notes (VJOURNAL)** — Note-taking with CalDAV backend
- **Gitea Issues** — Issue tracking across Gitea repositories

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
│           │   ├── network/                  # CalDavClient, GiteaClient, NextcloudSsoHelper, NextcloudLoginFlow
│           │   ├── parser/                   # ICalParser, TaskInputParser
│           │   ├── repository/               # EventRepository, TaskRepository, NoteRepository, IssueRepository, StatsRepository
│           │   ├── prefs/                    # SecureStore (Tink + Keystore), AppPreferences (DataStore)
│           │   └── di/                       # Hilt modules
│           ├── domain/
│           │   └── model/                    # Models.kt — all domain data classes
│           ├── ui/
│           │   ├── theme/                    # Theme.kt, Type.kt, Shape.kt
│           │   ├── navigation/               # AppNavigation.kt, Destination.kt
│           │   ├── adaptive/                 # FoldableUtils.kt
│           │   ├── screen/                   # dashboard/, events/, tasks/, notes/, issues/, inbox/, views/, settings/
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
| Nextcloud SSO | Android-SingleSignOn (JitPack) | 1.3.4 |
| Markdown (GFM) | multiplatform-markdown-renderer (mikepenz) | 0.39.2 |

### Adaptive UI Tiers

| Window | Width | Navigation | Layout |
|---|---|---|---|
| Phone | Compact (<600dp) | `NavigationBar` (bottom) | Single pane |
| Tablet / unfolded foldable | Medium (600–840dp) | `NavigationRail` | List + Detail side-by-side |
| Landscape tablet | Expanded (>840dp) | `NavigationDrawer` (persistent) | Sidebar + content |

`NavigationSuiteScaffold` auto-switches based on `WindowSizeClass`. List-detail screens use `NavigableListDetailPaneScaffold` on Medium/Expanded.

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
│   ├── CrossDashboardApp.swift     # @main, WindowGroup + Settings scene, AppContainer init
│   ├── AppContainer.swift          # all singletons; EnvironmentKey
│   ├── Data/
│   │   ├── Persistence/            # PersistenceController (SwiftData ModelContainer + @Model classes)
│   │   ├── Network/                # CalDavClient, GiteaClient, NextcloudLoginFlow
│   │   ├── Parser/                 # ICalParser, TaskInputParser
│   │   ├── Repository/             # EventRepository, TaskRepository, NoteRepository, IssueRepository, StatsRepository
│   │   └── Prefs/                  # KeychainStore (SecItem* wrapper), AppPreferences (@Observable UserDefaults)
│   ├── Domain/
│   │   └── Models.swift            # All domain structs/enums — Codable; mirrors Models.kt 1:1
│   ├── UI/
│   │   ├── App/                    # ContentView (NavigationSplitView), AppViewModel
│   │   ├── Screens/                # Dashboard/, Events/, Tasks/, Notes/, Issues/, Inbox/, Views/, Settings/, BiometricLockView.swift
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
| Markdown | `swift-markdown-ui` (gonzalezreal, SPM) |
| DI | `AppContainer` singleton + `@Environment` |

### Navigation

Three-column `NavigationSplitView`: sidebar (screen list) → content (list view) → detail (item detail). Replaces Android's `NavigationSuiteScaffold` + `NavigableListDetailPaneScaffold`. Deep link via `crossdashboard://` URL scheme (`onOpenURL`).

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

### Nextcloud Auth (three options)
1. **Nextcloud SSO** (`NextcloudSsoHelper`) — AIDL IPC via `Android-SingleSignOn`; preferred when NC app is installed (e.g. LineageOS + F-Droid).
2. **Login Flow v2** (`NextcloudLoginFlow`) — browser-based, no NC app required; polls every 2s for up to 5 min.
3. **Manual CalDAV** — server + username + password.

### Navigation (Nav3)
Type-safe `sealed class Destination : Parcelable` with `@Parcelize`. `AppNavigation.kt` uses `NavDisplay` + `NavigationSuiteScaffold`. Deep link `crossdashboard://tasks?action=add` handled in `MainActivity.onNewIntent` → sets `autoFocusQuickInput = true` on `TasksViewModel`.

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

### Markdown Rendering (GFM)
Read-only detail views render descriptions and note bodies as GitHub-Flavoured Markdown via `multiplatform-markdown-renderer` (Mike Penz, v0.39.2). The library is pure Compose — no `AndroidView` wrapper.

**Key components in `ui/component/`:**
- `MarkdownText(content, modifier)` — low-level composable. Colors are mapped to `MaterialTheme.colorScheme` tokens; typography to the M3 type scale. Images load via the app's existing Coil 3 instance (`Coil3ImageTransformerImpl`).
- `ReadMarkdownField(label, value, modifier)` — drop-in replacement for `ReadField` when the value may contain markdown. Shows the same small-caps label but renders the body through `MarkdownText`.

**Where markdown is rendered (read-only paths only):**

| Surface | Field | Component used |
|---|---|---|
| Task detail (`TaskReadView`) | Description | `ReadMarkdownField` |
| Event detail (`EventPropertySheet` + `EventDetailContent`) | Description | `ReadMarkdownField` |
| Note detail (`NoteReadView`) | Body | `MarkdownText` directly |
| Issue detail (`IssueReadContent`) | Description (body) | `MarkdownText` directly |
| Issue detail (`CommentItem`) | Comment body | `MarkdownText` directly |

**Rule:** Use `ReadMarkdownField` / `MarkdownText` only in read-only branches. Edit forms (`TaskEditForm`, `NoteEditForm`, etc.) always use plain `OutlinedTextField` — never pass markdown-rendered content into an editor.

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
| Settings | `SettingsViewModel` | CalDAV (SSO/LoginFlow/Manual), calendars, Gitea, theme, nav toggles, Pomodoro, alarms, widget sync, biometric |

Property sheets: `ModalBottomSheet` on phone; inline detail pane (`NavigableListDetailPaneScaffold`) on tablet/Expanded.

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

### Native Android — All 6 phases complete ✅

**Phase 1** — Data layer: Room DB (5 entities + DAOs), `SecureStore`, `AppPreferences`, `CalDavClient` + `ICalParser`, `GiteaClient`, `TaskInputParser`, all Repositories, Hilt DI modules.

**Phase 2** — Core UI: `MainActivity` (edge-to-edge, splash), Nav3 type-safe routes, `NavigationSuiteScaffold` adaptive scaffold, Material 3 dynamic color theme, `DashboardScreen`, `TasksScreen` with quick input + nested subtree, `PropertySheet` component.

**Phase 3** — Remaining screens: `EventsScreen`, `NotesScreen`, `IssuesScreen` (comments), `InboxScreen` (time totals), `ViewsScreen` (Kanban + Covey + assign modal), `SettingsScreen` (all sections including Nextcloud SSO + Login Flow v2).

**Phase 4** — Background: `SyncWorker` (WorkManager), `EventAlarmScheduler` + `EventAlarmReceiver`, `BootReceiver` + `AlarmPermissionReceiver`, Glance widget (`DashboardWidget`) with `SizeMode.Responsive` adaptive rows.

**Phase 5** — Live notifications: Notification channels, `PomodoroForegroundService` with `Notification.ProgressStyle` live update (API 36), `PomodoroViewModel` + `PomodoroBar` + `PomodoroModal`, session logging to task description.

**Phase 6** — Polish: `NavigableListDetailPaneScaffold` on all list screens (events/tasks/notes/issues), `FoldableUtils.kt` hinge avoidance, predictive back in `PropertySheet`, shared element transitions on Tasks, full TalkBack accessibility audit, dark mode refinement, ProGuard rules.

**Phase 7** — Issues enhancements: `CreateIssueSheet` (create issue with title, body, repo selector, file attachments via SAF), `PendingAttachment` UI data class, `GiteaAttachment` domain model, `AttachmentLink` composable, `GiteaClient` multipart upload/fetch methods for issue and comment assets, attachment display in `IssueReadContent` (issue-level section) and `CommentItem` (per-comment), attachment support in comment input bar.

### Native macOS — All 6 phases complete ✅

**Phase 1** — Data layer: SwiftData `PersistenceController` (5 `@Model` types), `KeychainStore`, `AppPreferences`, `CalDavClient` + `ICalParser`, `GiteaClient`, `TaskInputParser`, all Repositories, `AppContainer`.

**Phase 2** — Core UI: `CrossDashboardApp` (`@main`), `ContentView` (`NavigationSplitView` 3-col), `AppViewModel`, theme, `DashboardView`, `TasksView` + `QuickInputBar` + `TaskDetailView`, shared components (`PropertyDetailShell`, `ReadField`, `CalendarColorDot`, `StatusBadge`, `PriorityChip`, `TagChip`).

**Phase 3** — Remaining screens: `EventsView`, `NotesView`, `IssuesView` (comments + attachments via `NSOpenPanel`), `InboxView`, `ViewsView` (Kanban + Covey), `SettingsView` (all sections; Login Flow v2 + Manual only — no SSO), `ReadMarkdownField` / `ReadMarkdownView` via `swift-markdown-ui`.

**Phase 4** — Background: `SyncScheduler` (`NSBackgroundActivityScheduler`), `NotificationScheduler` (`UNCalendarNotificationTrigger`), on-launch rescheduling (mirrors `BootReceiver`), `UNUserNotificationCenterDelegate`.

**Phase 5** — Pomodoro: `PomodoroViewModel` (`@Observable` singleton, `Timer`-based), `PomodoroStatusItem` (`NSStatusItem` + `NSPopover`), `PomodoroBar` floating panel + `PomodoroModal`, session logging to task description.

**Phase 6** — Polish: `DashboardWidgetExtension` (WidgetKit, App Group shared SwiftData store, three `WidgetFamily` sizes), `BiometricLockView` (Touch ID + 6-digit PIN, SHA-256 via CryptoKit), toolbar + keyboard shortcuts (`Cmd+N`, `Cmd+R`, `Cmd+Delete`), VoiceOver accessibility audit, dark mode polish, notification tap navigation.

### Remaining / Next Steps
- Android: end-to-end smoke test (`./gradlew assembleDebug`) — fix any remaining compilation errors
- Android: shared element transitions for Events, Notes, Issues screens (lower priority)
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
3. Extend `CalDavClient` or `GiteaClient` in `data/network/` for new API calls.
4. Update the relevant Repository with new methods.
5. Add ViewModel logic (`@HiltViewModel`), exposing `StateFlow<UiState>`.
6. Build Compose UI in the appropriate `ui/screen/` directory.
7. Wire new routes in `Destination.kt` + `AppNavigation.kt` if a new screen.

### Credentials
- Use `SecureStore.set/get/delete` (Android) or `KeychainStore.set/get/delete` (macOS) for all sensitive values. Never pass raw passwords/tokens beyond the point of initial entry.

### UI (Android)
- Always support dark and light mode using `MaterialTheme.colorScheme` tokens.
- Use `NavigationSuiteScaffold` + `NavigableListDetailPaneScaffold` patterns for adaptive layouts.
- Every interactive composable must have a `contentDescription` for TalkBack.
- Wrap bottom sheets in `ModalBottomSheet` with `WindowInsets.ime` padding for keyboard avoidance.
- For description/body fields in **read-only** detail views, use `ReadMarkdownField` (labelled) or `MarkdownText` (unlabelled). Edit forms always use `OutlinedTextField`.

### Background Work (Android)
- Use `WorkManager` for all deferrable background tasks. Inject with `@HiltWorker`.
- After any sync, call `EventAlarmScheduler.rescheduleAll()` and `GlanceAppWidgetManager.updateIf<DashboardWidget>()`.

### Adding a Feature (macOS)
1. Define/update domain types in `Domain/Models.swift` if new data structures are needed.
2. Add a SwiftData `@Model` class in `Data/Persistence/` if persistence is required; add `init(from:)` + `toDomain()` mappers.
3. Extend `CalDavClient` or `GiteaClient` in `Data/Network/` for new API calls.
4. Update the relevant Repository with new methods.
5. Add `@Observable @MainActor` ViewModel logic, exposing published state.
6. Build SwiftUI views in the appropriate `UI/Screens/` directory.
7. Wire new navigation cases in `ContentView.swift` and `AppViewModel.swift`.
8. Run `xcodegen generate` in `native-macos/` after adding/removing Swift files.

### UI (macOS)
- Use `Color(.label)`, `Color(.secondaryLabel)`, `Color(.windowBackgroundColor)` — no hardcoded hex or `.black`/`.white`.
- Navigation via `NavigationSplitView` three-column; detail shown inline (no sheets for primary detail).
- Every interactive element needs `accessibilityLabel` / `accessibilityValue` / `accessibilityHint` for VoiceOver.
- For description/body fields in **read-only** detail views, use `ReadMarkdownField` (labelled) or `ReadMarkdownView` (unlabelled) via `swift-markdown-ui`. Edit forms always use `TextField` / `TextEditor`.
- Prefer toolbar `ToolbarItem` over FABs; add `keyboardShortcut` for common actions.
