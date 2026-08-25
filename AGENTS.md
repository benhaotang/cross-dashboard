# Cross-Dashboard — Agent Guide

Cross-Dashboard is a personal productivity dashboard integrating CalDAV events (VEVENT),
CalDAV tasks (VTODO), CalDAV notes (VJOURNAL), Gitea issues, and quick-capture notes from a
self-hosted [Memos](https://usememos.com) server. Three fully native apps ship from this repo:

| Platform | Directory | Stack |
|---|---|---|
| Android | `native-android/` | Kotlin + Jetpack Compose (Material 3) |
| macOS | `native-macos/` | SwiftUI |
| Linux | `native-linux/` | C++23 + GTK3 (gtkmm) + libhandy-1 |

There is no JavaScript/React shell. Authoritative version pins live in each platform's build
file (`gradle/libs.versions.toml`, `native-macos/project.yml`, `native-linux/meson.build`) —
consult those instead of trusting cached numbers here.

## Shared Design (all platforms)

### Data flow

The local database is the source of truth. UI observes the DB directly and renders instantly;
a background sync fetches fresh data and replaces the cache in a transaction so readers never
observe a temporary empty state. Repositories own sync; ViewModels expose state; composables /
views never call network or DB directly.

### Screens and naming

Eight visible screens in canonical order plus always-last Settings:

```
Dashboard, Inbox, Events, Tasks, Notes, Issues, Views, Capture
```

- This ordered list is the screens constant (`ALL_SCREENS` in Android `domain/model/Models.kt`,
  `allScreens` in macOS `Domain/Models.swift`, `kAllScreens` in Linux `domain/models.h`); users
  can reorder or hide screens in Settings, and the persisted `visibleScreens` preference stores
  the **ordered**, comma-separated selection. Render screens in stored order, never enum order.
- The Memos-backed screen displays as **"Capture"** everywhere user-visible. Internal
  identifiers keep the Memos name (Android `Destination.Memos`, macOS `Screen.memos` with
  `rawValue "Capture"`, Linux `memos` screen classes). Do not create a new destination and do
  not show the string "Memos" in any label.

### Network clients

Every platform implements the same four clients against its HTTP stack:

- **CalDavClient** — raw HTTP with non-standard methods (`PROPFIND`, `REPORT`, `MKCALENDAR`);
  no REST library abstraction fits these.
- **ICalParser** — hand-written RFC 5545 line parser (no third-party iCal library), reading and
  serializing VEVENT / VTODO / VJOURNAL.
- **GiteaClient** — REST DTOs with `toDomain()` mappers; issue and comment attachments upload
  as `multipart/form-data` with a single form field named `attachment`.
- **MemosClient** — Memos REST API v1 (`/api/v1/…`), `Authorization: Bearer <token>`; base URL
  from the credential store with trailing slash stripped.
- **KarakeepClient** — creates link bookmarks via `/api/v1/bookmarks`, loads folders via
  `/api/v1/lists`, adds bookmarks to manual folders via `PUT /api/v1/lists/{listId}/bookmarks/{bookmarkId}`.

**Memos proto3 JSON rule:** the server omits fields holding default values (false, empty string,
empty array). Every Memos DTO field must be nullable/optional with a default applied in the
mapper — on macOS a synthesized `Decodable` ignores default values, so a non-optional
`String` field throws `keyNotFound` whenever the server omits it.

**Memos attachment URLs:** `{host}/file/{att.name}/{att.filename}`, e.g.
`/file/attachments/gUeHhaXss87/photo.png`. `att.name` already contains the `attachments/`
prefix — use it verbatim.

### Credentials

Sensitive values go through the platform credential store only — Android `SecureStore`
(Tink AES-256-GCM + Keystore, backup-excluded file), macOS `KeychainStore` (`SecItem`),
Linux `SecretStore` (libsecret-1). Never write credentials to plain preferences, never log them,
never hardcode them. Non-sensitive settings use the platform prefs layer (DataStore /
UserDefaults / GKeyFile).

Credential keys are stable strings shared across platforms: `caldav_server`, `caldav_username`,
`caldav_password`, `caldav_auth_method`, `caldav_selected_calendars`,
`caldav_default_event_calendar`, `caldav_default_task_calendar` (`caldav_default_note_calendar`
is Linux-only), `nextcloud_sso_account` (not on macOS), `gitea_token`, `gitea_instance`,
`gitea_repos`, `memos_host`, `memos_token`, `karakeep_host`, `karakeep_token`, plus
`biometric_pin_hash` (macOS only). Two formats matter:

- `gitea_repos`: Android writes and reads a comma-separated list. macOS and Linux write a JSON
  array but their readers accept both forms (decode JSON first, fall back to comma-split).
- `caldav_selected_calendars` and the per-component default-calendar keys
  (`caldav_default_event_calendar`, `caldav_default_task_calendar`,
  `caldav_default_note_calendar`) select which calendars sync and which receive new
  events/tasks/notes.

### Nextcloud authentication

Three options on Android (Nextcloud SSO via AIDL, Login Flow v2 polling every 2 s for up to
5 min, manual server + username + password); Login Flow v2 + manual only on macOS and Linux.

### Task quick input

`TaskInputParser` (same grammar on all platforms):

```
!! meet friends #social tonight    → priority medium, tag social, due today evening
!!! deploy hotfix tomorrow morning → priority high, due tomorrow morning
buy milk #errands                  → tag errands only
call mom monday                    → due next Monday
```

`!`/`!!`/`!!!` map to priority 9/5/1 (low/medium/high); `#word` becomes a category;
time-of-day defaults come from `TaskDefaults` in Settings.

### Markdown and display math

Read-only detail views render descriptions and note bodies as GitHub-Flavoured Markdown:

- Only `$$…$$` renders as display math (centered, extra vertical spacing).
- Inline `$…$` stays literal on every platform: escape it before markdown parsing so LaTeX
  underscores do not trigger emphasis. Android/macOS split it out via `LatexContentParser`;
  Linux disables MathJax inline delimiters and escapes inside `markdown_view.cpp` (WebKitGTK) —
  inline rendering was dropped there deliberately over a line-height bug.
- Edit forms always use plain text fields, never the markdown renderer.

Per-platform components: Android `ReadMarkdownField` (labelled) / `MarkdownText` +
`LatexContentParser`; macOS `ReadMarkdownField` / `ReadMarkdownView` (swift-markdown-ui) +
SwiftMath; Linux `MarkdownView` (WebKitWebView + MathJax). Apply this only in read branches.

### Tags, milestones, and list filters

- Tags render as wrapping pill flows on list cards, detail views, and Inbox cards (Android
  `TagFlow`, macOS `TagChip`, Linux `make_tag_flow`). Three visual kinds: planning-duration
  tags (`#30m`, `#2h`), magic planning tags (user-configured Kanban tags plus the fixed Covey
  tags `do`, `delay`, `delegate`, `eliminate`), and ordinary tags. Keep this classification
  consistent on any new tag surface. Duration tags feed Inbox per-card time estimates and the
  total estimated-time summary.
- Issues persist assigned milestone ID, title, and due date on `GiteaIssue`; milestone identity
  is repository-scoped (`"owner/repo:id"`). Milestones are issue metadata and a filter — never
  standalone Inbox items.
- Filter model: Issues has Status, searchable multi-label, and (when detected) searchable
  milestone filters; Tasks and Capture have Status plus searchable multi-tag filters; multiple
  tags intersect (AND). A single ✕ resets the screen's filters. Inbox and Views compose an
  independent Type filter with a **Time range** filter (All / Today / Tomorrow / This week):
  events use start, tasks use due date, issues use milestone due date; undated items drop out
  only while a date filter is active.
- Filter buttons keep their category in the label (`Status – Open`) and get an accent treatment
  only when the selection differs from the screen default. Controls: Android `AdaptiveFilterBar`
  (dropdown menus at medium+ width, one modal sheet on phones); macOS and Linux reusable
  `SearchableFilterMenu` popovers (Linux: 360 px content width — do not shrink). Filter buttons
  carry a native semantic icon on Android (Material) and macOS (SF Symbols); Linux filters stay
  text-only because symbolic-icon availability varies by theme.
- Inbox rows navigate to the underlying event, task, or issue; preserve stable entity
  identifiers when changing row models.

### Desktop background snapshots

Inbox and Views offer a snapshot action that persists the current filters (and Views mode) as a
background template; data re-filters from the DB after each sync and later UI filter edits never
mutate the saved template. Settings can attach one backdrop picture shared by all profiles,
drawn with the chosen Scale/Fill/Stretch mode behind matte-glass panels (opacity default 80 %,
adjustable 50–100 %; white-tinted glass in light output, black-tinted in dark). Without a
picture, solid light/dark backgrounds render.

Hard rules:

- Render **titles, dates, types, tags, and counts only**. Descriptions, issue bodies,
  credentials, and attachment data must never reach a background render model.
- User-facing documentation must warn that titles may appear behind desktop icons or on the
  Android lock screen.
- Accents come from the system: Android's dynamic/Material accent, macOS `controlAccentColor`.
- Backdrop pictures have Both/Light/Dark slots; appearance-specific slots override Both.
  For a multi-image HEIC/HEIF imported into Both (macOS), ImageIO frame 0 is the light endpoint
  and the final frame the dark one; single-image files serve both variants.

Platform delivery: Android registers `DashboardWallpaperService` (redraws on `UI_MODE_NIGHT`;
separate standard/fold-cover/fold-inner templates); macOS renders per-`NSScreen` PNG pairs into
`~/Pictures/Cross-Dashboard/Backgrounds` and applies them with `NSWorkspace` (only the currently
addressable Space updates; sandbox holds a Pictures entitlement); the Linux systemd service
renders `$XDG_CACHE_HOME/crossdashboard/background.png` and invokes a direct argv command
containing `%f` (xwallpaper/swaybg presets; Flatpak cannot run host wallpaper commands). When
the app theme is Auto, the Linux service reads the XDG portal
`org.freedesktop.appearance/color-scheme` before rendering and subscribes to `SettingChanged`.
Android resolves the slot from the current `UI_MODE_NIGHT` configuration.

---

## Native Android (`native-android/`)

Android 16 (`minSdk`/`targetSdk`/`compileSdk` 36), Kotlin 2.1, Jetpack Compose, Hilt DI.
Package root: `app/src/main/kotlin/com/crossdashboard/app/`.

Layout owners:

```
data/db/          Room: AppDatabase (6 entities incl. Memo), DAOs; migrations wired in data/di/DataModule.kt
data/network/     CalDavClient, GiteaClient, MemosClient, KarakeepClient, NextcloudSsoHelper, NextcloudLoginFlow, SsoResultBus
data/parser/      ICalParser, TaskInputParser
data/repository/  Repositories.kt (aggregated) + MemoRepository.kt
data/prefs/       SecureStore (+ CredentialKey), AppPreferences (DataStore), AppTimeZone
domain/model/     Models.kt — all domain types
ui/navigation/    Destination.kt, AppNavigation.kt
ui/screen/        dashboard/, inbox/, events/, tasks/, notes/, issues/, views/, memos/, settings/
ui/component/     PropertySheet, MarkdownText, LatexContentParser, ReadMarkdownField, AdaptiveFilterBar, TagFlow, PomodoroBar, PomodoroModal, BiometricLockScreen, BackgroundSnapshotAction, …
service/          PomodoroForegroundService, PomodoroCommandBus
worker/           SyncWorker (@HiltWorker)
alarm/            EventAlarmScheduler/Receiver, TaskAlarmScheduler/Receiver
receiver/         BootReceiver, AlarmPermissionReceiver
widget/           DashboardWidget (Glance), DashboardWidgetReceiver, WidgetStateDefinition (+ WidgetStateStore)
background/       Wallpaper renderer + DashboardWallpaperService, profile resolver
```

### Navigation and adaptivity

Type-safe kotlinx.serialization routes (`Destination`) driven by `NavDisplay` +
`NavigationSuiteScaffold`: bottom `NavigationBar` below 600 dp, `NavigationRail` on medium
(600–840 dp), persistent `NavigationDrawer` above 840 dp. List-detail screens use
`NavigableListDetailPaneScaffold` at medium+. On phone width, more than five destinations
collapse into five buttons plus a "More" `DropdownMenu` — handled entirely in `AppNavigation.kt`.

Deep links arrive in `MainActivity.onNewIntent` and resolve to pending-action strings consumed
by the nav graph: `crossdashboard://tasks?action=add_task` focuses the Tasks quick input,
`crossdashboard://pomodoro` shows the running timer, and alarm/reminder notifications tap
through `crossdashboard://events?uid=…` / `crossdashboard://tasks?uid=…`.

### Background work

`SyncWorker` (WorkManager) refreshes repositories (guarded `deleteAll()` + `upsertAll()`), then
reschedules event and task alarms and pushes the widget. Exact alarms use the auto-granted
`USE_EXACT_ALARM` permission with stable IDs `abs(uid.hashCode()) % 100_000`; `BootReceiver`
reschedules after reboot.

Glance widget `DashboardWidget` uses `SizeMode.Exact`; its state serializes through a DataStore
serializer; post-sync updates call `DashboardWidget().update()` per glance ID.

### Pomodoro

`PomodoroViewModel` (`@Singleton`) owns all timer state. `PomodoroForegroundService` posts an
Android 16 promoted ongoing notification (`setRequestPromotedOngoing(true)`, chronometer
countdown while running) so a chip shows in the status bar. `PomodoroBar` pops up while active.
Session logging appends to the task description via `CalDavClient.updateTask()`.

### Conventions

- Kotlin only; strict null safety. ViewModels are `@HiltViewModel` exposing
  `StateFlow<UiState>`; suspend data-layer work runs on `Dispatchers.IO`; errors surface as
  `Result<T>` or a sealed error state, never silently.
- Colors come from `MaterialTheme.colorScheme` only. Every interactive element carries a
  `contentDescription`; compound rows use `semantics(mergeDescendants = true)`.
- Bottom sheets wrap in `ModalBottomSheet` with `WindowInsets.ime` padding.
- Naming: files/classes PascalCase, ViewModels `<Screen>ViewModel`, repositories
  `<Entity>Repository`, workers/receivers/services take a descriptive suffix.

### Issue attachments

`PendingAttachment(fileName, mimeType, bytes)` lives in `IssuesViewModel.kt`. Read bytes from a
`content://` Uri via `ContentResolver.openInputStream()` on `Dispatchers.IO`, and resolve
filenames with `OpenableColumns.DISPLAY_NAME` — `uri.lastPathSegment` returns internal document
IDs like `msf:162`. Issue attachments upload after `createIssue()` succeeds, comment attachments
after `addComment()`; both POST multipart to `/assets` endpoints. `AttachmentLink` renders each
fetched attachment and opens its download URL via `Intent.ACTION_VIEW`. The comment input bar
picks files with `ActivityResultContracts.GetContent("*/*")` and shows removable `InputChip`s.

### Share intake

`AndroidManifest.xml` declares share intent filters for `text/plain` and `*/*` (plus
`ACTION_SEND_MULTIPLE` `*/*`) routed to the Memos share extension; apply the DISPLAY_NAME rule
above when importing shared files.

### Commands

```bash
cd native-android
./gradlew assembleDebug   # build debug APK
./gradlew installDebug    # install on device/emulator
./gradlew lint test       # lint + unit tests
```

### Adding a feature

1. Extend domain models in `domain/model/Models.kt`.
2. Add Room entity + DAO and `toDomain()`/`toEntity()` mappers in `data/db/` + `Mappers.kt`;
   for schema changes bump `AppDatabase.version` and wire a `Migration` in
   `data/di/DataModule.kt`.
3. Extend the relevant client in `data/network/`, then the repository.
4. Add ViewModel logic exposing `StateFlow<UiState>`.
5. Build Compose UI under `ui/screen/<screen>/`; wire new routes in `Destination.kt` +
   `AppNavigation.kt`.
6. Verify with `./gradlew assembleDebug` (and `lint`, `test` where applicable).

Remaining Android work: end-to-end `assembleDebug` smoke pass; shared element transitions on
Events, Notes, Issues, Capture (lower priority).

---

## Native macOS (`native-macos/`)

SwiftUI app built via xcodegen (`project.yml`). Six targets: `CrossDashboardKit` (static
framework owning `Domain/`, `Data/Persistence/`, `Data/Prefs/`; linked, not embedded, by the
other targets), `CrossDashboard` (app), `CrossDashboardAgent`, `DashboardWidgetExtension`,
`MemosShareExtension`, `CrossDashboardTests`. All targets compile with Swift 6 strict
concurrency (`SWIFT_STRICT_CONCURRENCY = complete`). Deployment target sits at `"15.0"` until
the Xcode 26 SDK ships, then bumps to `"26.0"`.

Layout owners (under `CrossDashboard/`):

```
App/               AppDelegate (Services provider), AppViewModel (cross-screen triggers), ContentView (3-column root)
Data/Persistence/  PersistenceController — SwiftData ModelContainer + six @Model classes
Data/Network/      CalDavClient, GiteaClient, MemosClient, KarakeepClient, NextcloudLoginFlow
Data/Parser/       ICalParser, TaskInputParser
Data/Repository/   One repository per aggregate
Data/Prefs/        KeychainStore, AppPreferences (UserDefaults/@Observable), AppTimeZone
Domain/            Models.swift, TimerDeepLink, PersistedPomodoroSession, WidgetDataStore, BackgroundServiceContract
UI/App/            ContentView, AppViewModel
UI/Screens/        Dashboard/, Events/, Tasks/, Notes/, Issues/, Inbox/, Views/, Memos/, Settings/, BiometricLockView
UI/Components/     PropertyDetailShell, ReadField, ReadMarkdownField, MathView, LatexContentParser, CalendarColorDot, StatusBadge, PriorityChip, TagChip, PomodoroBar, SearchableFilterMenu
Background/        SyncScheduler, NotificationScheduler, DesktopBackground*, BackgroundServiceController
Pomodoro/          PomodoroViewModel (@Observable singleton), PomodoroStatusItem (NSStatusItem + NSPopover)
CrossDashboardAgent/ Service-owned sync, widget/background refresh, notifications, and Pomodoro phases
```

### Navigation and deep links

Three-column `NavigationSplitView`: sidebar (screens) → content (list) → detail (inline item
detail; primary detail never opens a sheet). Incoming `crossdashboard://` URLs reuse the
existing window via `.handlesExternalEvents(matching: ["*"])` on `WindowGroup` plus
`preferring/allowing: ["crossdashboard"]` on `ContentView`.

Handled links:

- `tasks?action=add` → focus Tasks quick input
- `capture?text=<percent-encoded>` → open Capture compose sheet pre-filled
- `timer` (= alias `pomodoro`) → idle picker; `timer?name=<n>&minutes=<1…1440>` starts a named
  timer; `timer?action=start&type=task&id=<uid>` links a cached active task;
  `timer?action=start&type=issue&id=<owner/repo#number>` (or numeric issue id) links an open
  issue; `action=pause|resume|toggle|stop|skip` controls the running timer, and idle `toggle`
  opens the picker. Name matching is case-insensitive and exact; unresolved targets open the
  picker rather than starting the wrong item.

Cold-start rule: triggers like `triggerCapture(text:)` can fire before `MemosView` appears, and
SwiftUI `.onChange` misses pre-appearance values — observe such triggers with **both**
`.onAppear` and `.onChange`.

### Background agent and widget sharing

- `CrossDashboardAgent.app` is the sole owner of periodic sync, widget snapshots, automatic
  wallpaper rendering, event-notification scheduling, and Pomodoro phase transitions. The GUI
  requests immediate sync and schedule reloads over XPC, then reloads SwiftData after the agent's
  distributed completion notification. Keep the GUI's `NSBackgroundActivityScheduler` only as a
  fallback while the service is disabled or unavailable.
- The agent is an `LSBackgroundOnly` app embedded in the main app Resources and registered with
  `SMAppService.agent(plistName:)` using `group.com.crossdashboard.background-agent.plist`.
  Keep the app wrapper: a bare executable lacks bundle identity under App Sandbox and crashes in
  `libsystem_secinit`. Inspect it with
  `launchctl print "gui/$(id -u)/group.com.crossdashboard.background-agent"`; healthy means
  `job state = running` plus a successful Settings connection test.
- Sync fetches distinguish an authoritative empty response from transport, HTTP, authentication,
  and decoding failure. Replace each SwiftData cache in one transaction only after every configured
  endpoint for that source succeeds. A failed or partial fetch retains the previous cache and does
  not advance `lastSyncDate`.
- Shared storage uses Team-ID App Group `569WLL4Q5F.com.crossdashboard`; legacy
  `group.com.crossdashboard` remains only for preference migration and the XPC service name.
  Put new shared data in the Team-ID group.
- `WidgetDataStore` atomically writes `widget-snapshot.json` there after each sync; the widget
  reads the file before its UserDefaults fallback, and `WidgetCenter` reloads all families.
  Keep the widget off the live SwiftData store; log failures under unified-log categories
  `WidgetSnapshot` / `WidgetTimeline`. Layouts: small = next event + icon-only task/issue
  counts; medium = event/task columns + issue footer; large = two five-row agenda columns +
  issue footer. No app title inside widget content.

### Platform behaviors

- Background: `NSBackgroundActivityScheduler` drives sync; `UNCalendarNotificationTrigger`
  schedules reminders; alarms reschedule on launch.
- Biometric lock: `LAContext` Touch ID + 6-digit PIN hashed with SHA-256 (CryptoKit) into the
  Keychain.
- Services menu (`NSServices` → `captureToMemos` in `AppDelegate`) only surfaces for Developer
  ID/App Store-signed builds — macOS 26 filters spctl-rejected debug builds. During development
  use `crossdashboard://capture?text=` instead.
- Share extension filenames: `NSItemProvider.suggestedName` is unreliable and
  `loadFileRepresentation` renames copies after the UTI description. Load `public.file-url`
  first and take `url.lastPathComponent` as the authoritative filename; use
  `loadFileRepresentation` only for bytes (see `loadOriginalFilename(from:)` in
  `MemosShareExtension/ShareViewController.swift`).
- Memos sheets live in `UI/Screens/Memos/` as `CommentOnIssueSheet.swift`,
  `ExtractTasksSheet.swift`, `CreateEventFromMemoSheet.swift`, `SaveToKarakeepSheet.swift`;
  `CreateMemoSheet` is a struct inside `MemosView.swift` (no separate file, no "Mac" suffixes).

### Conventions

- System-defined colors only (`Color(.label)`, `.secondaryLabel`, `.windowBackgroundColor`).
- Every interactive element gets `accessibilityLabel`/`accessibilityValue`/`accessibilityHint`.
- Prefer toolbar `ToolbarItem` with `keyboardShortcut` (`Cmd+N` new, `Cmd+R` refresh,
  `Cmd+Delete` delete); FAB-style floating buttons have no place on macOS.

### Commands

```bash
cd native-macos
xcodegen generate             # regenerate project after adding/removing Swift files
open CrossDashboard.xcodeproj # build + run in Xcode
```

### Adding a feature

1. Extend domain types in `Domain/Models.swift`.
2. Add a SwiftData `@Model` in `Data/Persistence/` with `init(from:)` + `toDomain()` mappers.
3. Extend the client in `Data/Network/`, then the repository.
4. Add `@Observable @MainActor` ViewModel logic.
5. Build SwiftUI views under `UI/Screens/<screen>/`; wire navigation in `ContentView.swift` +
   `AppViewModel.swift`.
6. Run `xcodegen generate`, then build and resolve any strict-concurrency errors.

Remaining macOS work: full build pass fixing Swift 6 concurrency warnings; deployment-target
bump to 26.0 when the SDK ships; "Change PIN" flow in Settings → Security.

---

## Native Linux (`native-linux/`)

C++23, GTK3 3.24 (gtkmm-3.0) + libhandy-1, Meson build. Debian 13 / Ubuntu 22.04 LTS+
targets; Flatpak primary packaging plus `.deb`. Notable dependencies: libsoup-3.0,
nlohmann/json, SQLite3 (WAL, 5-second busy timeout), libsecret-1, webkit2gtk-4.0 (falls back
to 4.1), libcmark-gfm + bundled MathJax, libnotify, and optionally
appindicator3/ayatana-appindicator3 (gated by `-DCD_HAVE_APPINDICATOR`; GNOME needs an
extension for indicators).

Installed artifacts: `cross-dashboard` (GUI), `cross-dashboard-cli`, and
`cross-dashboard-service` (libexecdir, systemd user service, `Type=dbus` on bus
`com.crossdashboard.Service`).

Layout owners (under `src/`):

```
main.cpp, application.*, app_container.*, cli_main.cpp, background_service_main.cpp
domain/models.h                 kAllScreens + all domain structs
data/db/                        Database + per-entity DAO headers + stats_dao
data/network/                   CalDavClient, GiteaClient, MemosClient, KarakeepClient, NextcloudLoginFlow
data/parser/                    ICalParser, TaskInputParser
data/repository/                Aggregated repositories + operation_lock, repo_utils
data/prefs/                     SecretStore (libsecret), AppPreferences (GKeyFile ~/.config/crossdashboard/prefs.ini)
ui/app_window.*, ui/app_viewmodel.*
ui/screens/                     dashboard/ events/ tasks/ notes/ issues/ inbox/ views/ memos/ settings/
ui/components/                  property_panel, markdown_view, read_markdown_field, searchable_filter_menu, tag_flow, pomodoro_bar/modal, memo_auth_image, attachment_row
background/                     service_dbus.h, sync_runner, sync_scheduler, notification_scheduler, pomodoro_session, pomodoro_status_item, background_* (renderer/manager/definition/content_builder/command)
data/waybar/                    Packaged Waybar module config.jsonc + style.css
```

### Process coordination

- `cross-dashboard-service` is the sole owner of periodic sync, event reminders, and the
  Pomodoro countdown. The GUI and CLI force refreshes and control the timer over D-Bus
  (`Sync`, `RefreshBackground`, `StartPomodoro`/`Get`/`Pause`/`Resume`/`Stop`/`SkipPomodoro`;
  signals `SyncCompleted`, `BackgroundUpdated`, `PomodoroStateChanged`).
- All processes mutate repositories under the per-user `OperationLock`; SQLite WAL plus the
  busy timeout and transactional `replace_all()` keep readers off the temporary empty state.
- The GTK Pomodoro popup picks from cached incomplete tasks/open issues before starting, then
  exposes pause/resume/stop/skip for the service-owned session. The packaged Waybar module
  (`custom/cross-dashboard` executing `cross-dashboard-cli waybar`, JSON output) mirrors live
  status.
- `cross-dashboard-cli` pipes smart-syntax tasks and captures (`echo 'buy milk' | cross-dashboard-cli task`),
  lists cached entities (`list tasks|events|issues [--all|--json|--fuzzel]`), forces sync, and
  drives the Pomodoro (`pomo status|pause|resume|stop|task|fuzzel|toggle`).
- Main window is a `HdyLeaflet`: persistent 240 px sidebar when wide, header-bar toggle + swipe
  when folded.

### Platform behaviors

- Settings → CalDAV offers component-filtered defaults for new events/tasks/notes using
  `caldav_default_event_calendar`, `caldav_default_task_calendar`, `caldav_default_note_calendar`;
  saved defaults survive rediscovery, join the selected sync set, and every creation flow
  (quick input, CLI, Notes, Capture event/task actions) prefers them over the first calendar.
- No home-screen widget and no biometric lock — intentional parity gaps; do not add them
  without asking first.
- Meson test suite: `parser_smoke`, `database_concurrency`, `cli_help`, `cli_fuzzel`,
  `background_command`, `background_renderer`, `service_pomodoro`.

### Conventions

- Colors via GTK3 CSS custom properties; dark/light toggled with
  `gtk-application-prefer-dark-theme`.
- Every interactive widget needs an AT-SPI2 accessible name (`atk_object_set_name`; tooltips
  are not sufficient). Detail views show inline as `PropertyPanel` at medium+ width and as
  `GtkDialog` when narrow. Prefer header-bar buttons with `GtkShortcutController` shortcuts over
  context menus for primary actions; use `GtkFileChooserNative` for pickers.

### Commands

```bash
cd native-linux
meson setup build                # first-time configure
meson compile -C build           # build
meson test -C build              # test suite (see list above)
meson install -C build           # install (DESTDIR=pkg meson install -C build to stage)
flatpak-builder --user --install --force-clean build-flatpak flatpak/com.crossdashboard.app.yml
dpkg-buildpackage -b -us -uc     # build .deb (from native-linux/)
docker build -f docker/Dockerfile -t cross-dashboard-linux .   # Debian build smoke (needs Docker daemon)
```

### Adding a feature

1. Extend domain types in `domain/models.h`.
2. Add a DAO in `data/db/` with `to_domain()`/`from_domain()` helpers if persistence is needed.
3. Extend the client in `data/network/`, then the repository.
4. Expose observable state from a ViewModel class via GObject signals or `std::function`.
5. Build GTK views under `ui/screens/<screen>/`; wire navigation cases in `AppWindow` +
   `AppViewModel`.
6. Verify with `meson compile -C build` and `meson test -C build`.

Remaining Linux work: dedicated per-DAO `.cpp` files, Gitea comment/attachment DB cache beyond
the parity stub, richer unit tests.

---

## Release checklist

Bump versions in all of:

1. `native-android/app/build.gradle.kts` — `versionName` + `versionCode` (Settings About reads
   `BuildConfig.VERSION_NAME` automatically; no separate edit needed)
2. `native-macos/project.yml` — `MARKETING_VERSION` (+ `CURRENT_PROJECT_VERSION`)
3. `native-linux/meson.build` — `version:` in `project()`
4. `native-linux/debian/changelog` — new entry with version and date
5. `native-linux/com.crossdashboard.app.metainfo.xml` — new `<release>` entry

## Security guardrails

Credentials enter the app once and live only in the platform credential store afterward.
Validate API responses before processing and log errors without sensitive fields. Re-check the
background-render rule above before touching anything snapshot-related.
