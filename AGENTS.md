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
| **Android** | ✅ Primary — complete | `native-android/` | Kotlin + Jetpack Compose |
| **macOS** | Planned | `native-macos/` (TBD) | SwiftUI or Compose Multiplatform |
| **Linux** | Planned | `native-linux/` (TBD) | Compose Multiplatform or GTK |
| React Native / Expo | Legacy — phased out | `src/`, `modules/` | TypeScript + Expo SDK 54 |

The RN/Expo codebase (`src/`) is frozen. All new development goes into `native-android/` first, then the macOS and Linux targets when they begin. Do not add new features to the RN code.

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
- `GiteaClient` uses kotlinx.serialization DTOs with `toDomain()` mappers.

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
| Issues | `IssuesViewModel` | State filter, comments, open/close toggle, Pomodoro |
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

### Remaining / Next Steps
- End-to-end smoke test: `./gradlew assembleDebug` — fix any remaining compilation errors
- Shared element transitions for Events, Notes, Issues screens (lower priority)
- macOS native target (TBD stack — SwiftUI or Compose Multiplatform)
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
- Use `SecureStore.set/get/delete` for all sensitive values. Never pass raw passwords/tokens through function parameters beyond the point of initial entry.

### UI
- Always support dark and light mode using `MaterialTheme.colorScheme` tokens.
- Use `NavigationSuiteScaffold` + `NavigableListDetailPaneScaffold` patterns for adaptive layouts.
- Every interactive composable must have a `contentDescription` for TalkBack.
- Wrap bottom sheets in `ModalBottomSheet` with `WindowInsets.ime` padding for keyboard avoidance.
- For description/body fields in **read-only** detail views, use `ReadMarkdownField` (labelled fields) or `MarkdownText` (unlabelled) — never plain `Text`. Edit forms always use `OutlinedTextField`.

### Background Work
- Use `WorkManager` for all deferrable background tasks. Inject with `@HiltWorker`.
- After any sync, call `EventAlarmScheduler.rescheduleAll()` and `GlanceAppWidgetManager.updateIf<DashboardWidget>()`.
