# Cross-Dashboard - AI Agent Instructions

## Project Overview

Cross-Dashboard is a React Native application providing a unified web dashboard for:
- **CalDAV Events** - Calendar synchronization and event management
- **Notes** - Note-taking with CalDAV backend storage
- **CalDAV Tasks** - VTODO-based task management with subtasks, intelligent quick input
- **Gitea Issues** - Issue tracking and management from Gitea repositories

### Target Platforms
- Android
- macOS (via React Native macOS or Expo)
- Web (via react-native-web)
- Linux AppImage (amd64 & aarch64, via Electron wrapper)

### Tech Stack
- **Framework**: React Native with Expo (SDK 54)
- **Language**: TypeScript (strict mode)
- **Package Manager**: pnpm (strictly enforced)
- **UI**: React Native components with platform-specific adaptations
- **Icons**: Iconify (`@iconify/react`) - MDI icon set
- **Navigation**: React Navigation (bottom tabs for mobile, sidebar for web/desktop)
- **State Management**: React Context (`src/store/AppContext.tsx`) — events, notes, tasks, issues, selectedCalendars, visibleScreens; Pomodoro context (`src/store/PomodoroContext.tsx`) — shared timer state + native module bridge
- **Secure Storage**: expo-secure-store (keyring integration)

---

## Architecture

### Directory Structure
```
cross-dashboard/
├── src/
│   ├── components/     # Reusable UI components (Icon, PropertyPageShared, *PropertyPage)
│   ├── navigation/     # Navigation configuration (AppNavigator, SidebarNavigator)
│   ├── screens/        # Screen components (Dashboard, Events, Notes, Tasks, Issues, Settings)
│   ├── services/       # API clients (caldav.ts, nextcloud.ts, gitea.ts, keyring.ts, taskParser.ts)
│   ├── hooks/          # Custom React hooks
│   ├── types/          # TypeScript type definitions (index.ts)
│   ├── utils/          # Helper utilities
│   └── store/          # State management (AppContext.tsx, PomodoroContext.tsx)
├── modules/            # Expo native modules
│   ├── unified-push/   # UnifiedPush client (Android)
│   ├── pomodoro-service/ # Pomodoro foreground service (Android)
│   └── dashboard-widget/ # Android home screen widget + WorkManager sync
├── electron/           # Electron main process (Linux AppImage builds)
│   └── main.js         # BrowserWindow loading Expo web export
├── assets/             # Images, fonts, etc.
├── App.tsx             # Root component with providers
├── electron-builder.yml # Electron-builder config for AppImage packaging
└── index.ts            # Entry point
```

### Security Guidelines
- **NEVER** hardcode credentials or API tokens
- All sensitive data (CalDAV passwords, Gitea tokens) MUST use `expo-secure-store`
- For web platform, use encrypted localStorage fallback with user-provided encryption key
- Validate all API responses before processing

### Key Services

#### CalDAV Service (`src/services/caldav.ts`)
- Authentication via stored credentials (manual or Nextcloud Login Flow v2)
- Calendar discovery via PROPFIND (`fetchCalendars()` → `CalDavCalendar[]` with href, displayName, color, components)
- Calendar-aware fetching: `fetchEvents`, `fetchTasks`, `fetchNotes` accept optional `calendarHrefs` parameter
- Each fetched event/task is tagged with `calendarHref` for calendar color identification
- Event CRUD operations (VEVENT)
- Calendar synchronization
- Notes storage (VJOURNAL fetch/create/update/delete)
- Task CRUD operations (VTODO with `fetchTasks`, `createTask`, `updateTask`, `deleteTask`)
- Subtask support via `RELATED-TO;RELTYPE=PARENT` property
- `createTask` and `createNote` accept optional `calendarHref` for target calendar selection

#### Nextcloud Service (`src/services/nextcloud.ts`)
- **Login Flow v2**: `initiateLoginFlow(serverUrl)` → `{pollToken, pollEndpoint, loginUrl}`
- **Credential polling**: `pollForCredentials(pollEndpoint, pollToken, signal?)` — polls every 2s for up to 5 min
- **CalDAV URL discovery**: `discoverCalDavUrl(serverUrl, username)` → `{base}/remote.php/dav/calendars/{user}/`
- CORS limitation: Login Flow v2 does not work on web platform (falls back to manual entry)

#### Task Parser (`src/services/taskParser.ts`)
- Intelligent single-line task input parser (Todoist-style)
- Priority: `!` (low), `!!` (medium), `!!!` (high) — maps to CalDAV priority numbers
- Tags: `#word` — maps to CalDavTask `categories[]`
- Time keywords: `today`, `tonight`, `tomorrow`, `tomorrow morning/afternoon/night`, weekday names (`monday`-`sunday`), `next week`
- Configurable time-of-day defaults (`TaskDefaults`): morning, afternoon, night, and bare-day hours
- Pure function with no React dependencies

#### Gitea Service (`src/services/gitea.ts`)
- Personal access token authentication
- Fetch issues from configured repositories
- Create/update issues (`updateIssue` supports title, body, state)
- Label and milestone management
- Issue comments: `fetchComments`, `addComment` (returns `GiteaComment[]`)

#### Keyring Service (`src/services/keyring.ts`)
- Wrapper around expo-secure-store
- Platform-specific implementations for secure credential storage
- Methods: `setCredential`, `getCredential`, `deleteCredential`, `hasCredential`, `clearAllCredentials`
- Web fallback uses localStorage (less secure)
- Credential keys include: `caldav_password`, `caldav_server`, `caldav_username`, `caldav_auth_method` (manual/nextcloud), `caldav_selected_calendars` (JSON CalDavCalendar[]), `caldav_default_event_calendar`, `caldav_default_task_calendar`, `gitea_token`, `gitea_instance`, `notif_*`, `up_endpoint`, `encryption_key*`

---

## UI Components

### Icon System (`src/components/Icon.tsx`)
- Uses `@iconify/react` for web platform
- Native fallback for Android (placeholder, can extend with react-native-vector-icons)
- Centralized icon names in `Icons` constant:
  ```typescript
  Icons.dashboard    // mdi:view-dashboard
  Icons.calendar     // mdi:calendar
  Icons.notes        // mdi:note-text
  Icons.task         // mdi:checkbox-marked
  Icons.taskOutline  // mdi:checkbox-blank-outline
  Icons.subtask      // mdi:subdirectory-arrow-right
  Icons.priority     // mdi:alert-circle
  Icons.issues       // mdi:bug
  Icons.settings     // mdi:cog
  Icons.refresh      // mdi:refresh
  Icons.add          // mdi:plus
  Icons.delete       // mdi:delete
  Icons.pencil       // mdi:pencil
  Icons.comment      // mdi:comment-text-outline
  Icons.play         // mdi:play-circle-outline
  Icons.pause        // mdi:pause-circle-outline
  Icons.stop         // mdi:stop-circle-outline
  Icons.timer        // mdi:timer-outline
  Icons.views        // mdi:view-column
  ```

### useSyncAll Hook (`src/hooks/useSyncAll.ts`)
Shared hook used by **every** screen's refresh button to avoid divergent sync logic:
- Fetches events, tasks, notes (CalDAV) and issues (Gitea) in parallel using `selectedCalendars`
- Guards against clearing existing data: only calls `setX(data)` if the new or old array is non-empty (prevents wipe on transient failure)
- Captures fresh data from fetch results (not stale `state.*`) before post-processing
- After sync: reschedules event reminders, updates Android widget (if available) with formatted event/task/issue rows
- All screens import `{ useSyncAll }` and call `const { syncAll } = useSyncAll()` for their refresh button

### Daily Activity Stats (`src/services/cache.ts`)
Local-only activity counter stored in AsyncStorage under `@cache/daily_stats`:
- `DailyStats` interface: `{ tasksCompleted, pomodoroSessions, issuesClosed }`
- `StatsStore` = `Record<'YYYY-MM-DD', DailyStats>` — one entry per day
- `incrementStat(stat)` — atomically increments one counter for today; called:
  - `tasksCompleted`: in `TasksScreen.toggleCompletion`, `TasksScreen.saveTask`, `TaskPropertyPage.onSave` (all guard against double-count by checking prior status)
  - `pomodoroSessions`: in `TaskPropertyPage.handlePomodoroSession`
  - `issuesClosed`: in `IssuesScreen.onStateToggle` when `newState === 'closed'`
- `sumStatRange(store, startDaysAgo, count)` — pure function, sums stats for a range of days
- `DashboardScreen` loads `statsStore` on mount and on `lastSync` change, computes `this7` (days 0–6) and `prev7` (days 7–13), shows delta arrows (↑/↓) if prior week has any data

### Android Widget Module (`modules/dashboard-widget/`)
Expo native module for the Android home screen widget:
- **`DashboardWidgetModule.kt`** — Expo Module with functions:
  - `updateWidgetData(eventRowsStr, taskRowsStr, issuesCount, lastSync)` — pipe-separated row strings (up to 3 each), triggers widget redraw
  - `saveWorkerCredentials(caldavServer, caldavUser, caldavPass, calendarHrefs, giteaUrl, giteaToken, giteaRepos)` — persists credentials to SharedPreferences for background worker use; called from SettingsScreen after CalDAV/Gitea save and calendar selection save
  - `scheduleSync(intervalMinutes)` — schedules/replaces periodic `PeriodicWorkRequest` (min 15 min, default 60); `ExistingPeriodicWorkPolicy.UPDATE`
  - `cancelSync()` — cancels the periodic work
  - `forceRefresh()` — triggers immediate widget redraw
- **`DashboardWidgetProvider.kt`** — `AppWidgetProvider`:
  - Size-aware: reads `OPTION_APPWIDGET_MIN_HEIGHT` from options bundle, shows 1–3 rows per section via `setViewVisibility()`
  - Handles `onAppWidgetOptionsChanged` (live resize)
  - FAB `PendingIntent` deep-links to `crossdashboard://tasks?action=add`
  - Content tap opens the app via launch intent
- **`WidgetSyncWorker.kt`** — `CoroutineWorker` (WorkManager):
  - Reads credentials from SharedPreferences (set by `saveWorkerCredentials`)
  - Makes CalDAV `REPORT` for upcoming VEVENTs (next 30 days) and pending VTODOs (non-COMPLETED)
  - Simple line-by-line iCal parser with fold unfolding; parses SUMMARY, DTSTART, DUE
  - Calls Gitea `/api/v1/repos/{repo}/issues?state=open` and reads `X-Total-Count` header
  - Writes formatted row strings + counts to SharedPreferences, broadcasts `ACTION_UPDATE_WIDGET`
- **`widget_dashboard.xml`** — `RelativeLayout` with EVENTS section (3 rows + empty fallback), TASKS section (3 rows + empty fallback), issues count + last-sync footer, and a circular `+` FAB (`Button` with `@drawable/fab_background` oval shape) pinned bottom-right
- **`index.ts`** — TypeScript wrapper with `isAvailable()`, `updateWidgetData()`, `saveWorkerCredentials()`, `scheduleSync()`, `cancelSync()`, `forceRefresh()`
- **SharedPreferences key** (`cross_dashboard_widget`): `event_row_0/1/2`, `task_row_0/1/2`, `events_count`, `tasks_count`, `issues_count`, `last_sync`, `worker_caldav_*`, `worker_gitea_*`, `worker_calendar_hrefs`, `worker_gitea_repos`

### Property Pages (`src/components/*PropertyPage.tsx`)
Full-screen modal overlays for viewing/editing item details. Built on shared components from `PropertyPageShared.tsx`:
- **PropertyPageModal** -- outer modal shell wrapped in `KeyboardAvoidingView behavior="padding"` (avoids keyboard on both iOS and Android without flying-too-high issue); `minHeight: '70%'`, `maxHeight: '92%'`
- **PropertyPageHeader** -- header with Close (left), title (center), pencil/cancel edit toggle (right)
- **ReadField** -- labeled read-only field row
- **SectionHeader** -- uppercase section divider
- **resolveCalendar()** -- resolves `calendarHref` to `{displayName, color}`

Property page components:
- **EventPropertyPage** -- read-only (no edit mode): date/time range, duration, location, description, UID
- **NotePropertyPage** -- read/edit toggle: tags as chips, full content, title/content/tags editing
- **TaskPropertyPage** -- read/edit toggle: status badge, priority, due date, progress bar, categories with Kanban quick-tag chips (defaults to `['backlog', 'planned', 'inprogress', 'done']` if none saved), subtask list with completion toggle, full edit form; Pomodoro play button logs sessions
- **IssuePropertyPage** -- read/edit toggle: state badge, labels, assignees, body, open/close toggle, comments section (fetched from Gitea API), add comment, "Open in Browser" link

### Navigation (`src/navigation/`)
- **AppNavigator.tsx**: Platform-aware navigation router
  - Web/macOS/Linux: Uses `SidebarNavigator`
  - Android: Uses bottom tab navigation with deep link support (`crossdashboard://` scheme)
  - Both navigators filter screens by `visibleScreens` from AppContext (Settings always shown)
  - `linking` config maps `crossdashboard://tasks?action=add` → TasksScreen with quick input focused
- **SidebarNavigator.tsx**: Sidebar navigation for desktop/web
  - Collapsible at <900px width
  - Shows icons and labels
  - Active state highlighting
- **Visible screens**: Configurable in Settings > Navigation; persisted via `cache.saveVisibleScreens()`
  - `ScreenName` type and `ALL_SCREENS` constant exported from `src/services/cache.ts`
- **Deep linking** (Android): `app.json` declares `crossdashboard://` intent filter; React Navigation `LinkingOptions` handles initial URL and warm-start navigation

### Screens (`src/screens/`)
| Screen | Description |
|--------|-------------|
| DashboardScreen | Overview with upcoming events, tasks due soon, open issues, and 7-day activity stats card (tasks completed, pomodoros, issues closed) with delta vs previous 7 days |
| EventsScreen | CalDAV events with day/week/month filters, calendar color dots, tap-to-open read-only property page |
| NotesScreen | Note management with create/edit/delete, calendar-aware fetching, tap-to-open property page with read/edit modes |
| TasksScreen | VTODO task management with nested subtrees, quick input bar (auto-focused on deep link `action=add`), CRUD modal, calendar color dots, tap-to-open property page with subtask list, read/edit modes |
| IssuesScreen | Gitea issues with state filtering, tap-to-open property page with comments, read/edit modes |
| ViewsScreen | Kanban board + Covey's Four Quadrants (Eisenhower matrix) — tasks and issues grouped by `#tag` (categories/labels), assign modal with mutual exclusivity, configurable kanban columns, syncs tag changes back to CalDAV/Gitea |
| InboxScreen | Unified inbox aggregating events, tasks, issues, and milestones; total estimated time calculation from event durations and `#Xm`/`#Xh` time tags on tasks/issues |
| SettingsScreen | Nextcloud login / manual CalDAV, calendar picker with color dots & component badges (inline refresh button), default event/task calendar selection, theme, visible screen toggles, task input defaults, Pomodoro settings, widget sync interval (Android), notifications |

---

## Coding Conventions

### TypeScript
- Strict mode enabled
- Define interfaces for all data structures in `src/types/`
- Use explicit return types for functions
- Avoid `any` type - use `unknown` when necessary

### React Native
- Functional components with hooks only
- Use `StyleSheet.create()` for styles
- Platform-specific code via `Platform.OS` or `.android.ts` / `.web.ts` extensions
- Keep components focused and composable

### Naming
- Components: PascalCase (`CalendarView.tsx`)
- Hooks: camelCase with `use` prefix (`useCalDavSync.ts`)
- Services: camelCase (`caldav.ts`)
- Types: PascalCase with descriptive names (`CalendarEvent`, `GiteaIssue`)

---

## API Integration Patterns

### CalDAV
```typescript
interface CalDavConfig {
  serverUrl: string;
  username: string;
  // password retrieved from secure store
}

interface CalDavCalendar {
  href: string;          // e.g. /remote.php/dav/calendars/user/personal/
  displayName: string;
  color?: string;        // from apple:calendar-color, normalized to #RRGGBB
  ctag?: string;         // change tag for cache invalidation
  components: string[];  // ['VEVENT', 'VTODO', 'VJOURNAL']
}

interface CalendarEvent {
  uid: string;
  summary: string;
  start: Date;
  end: Date;
  description?: string;
  location?: string;
  calendar?: string;
  calendarHref?: string;  // href of the calendar this event was fetched from
}

type TaskStatus = 'NEEDS-ACTION' | 'IN-PROCESS' | 'COMPLETED' | 'CANCELLED';

interface CalDavTask {
  uid: string;
  summary: string;
  description?: string;
  status: TaskStatus;
  priority: number;        // 0=undefined, 1-4=high, 5=medium, 6-9=low
  percentComplete: number; // 0-100
  due?: Date;
  dtstart?: Date;
  completed?: Date;
  created: Date;
  lastModified: Date;
  categories?: string[];   // tags from #word quick input
  location?: string;
  parentUid?: string;      // RELATED-TO;RELTYPE=PARENT (subtask support)
  calendarHref?: string;   // href of the calendar this task was fetched from
}
```

### Task Quick Input Syntax
```
!! meet friends #social tonight    → priority=medium, tag=social, due=today 21:00
!!! deploy hotfix tomorrow morning → priority=high, due=tomorrow 08:00
buy milk #errands                  → no priority, tag=errands, no due
call mom monday                    → due=next Monday 10:00
```
Time-of-day hours are configurable in Settings > Task Input.

### Gitea
```typescript
interface GiteaConfig {
  instanceUrl: string;
  // token retrieved from secure store
  repositories: string[]; // "owner/repo" format
}

interface GiteaIssue {
  id: number;
  title: string;
  body: string;
  state: 'open' | 'closed';
  labels: string[];
  assignees: string[];
  createdAt: Date;
  updatedAt: Date;
}
```

---

## Version Bumping

When bumping the app version, update **all three** locations:
1. `package.json` — `"version"` field
2. `app.json` — `"expo"."version"` field
3. `src/screens/SettingsScreen.tsx` — version string in the About section (`Cross-Dashboard vX.Y.Z`)

---

## Development Commands

```bash
# Install dependencies
pnpm install

# Start development server
pnpm start

# Platform-specific
pnpm android    # Android emulator/device
pnpm web        # Web browser (sidebar navigation)

# Type checking
pnpm typecheck

# Build Linux AppImage (requires electron + electron-builder)
pnpm electron:build

# Fix Expo SDK compatibility issues
npx expo install --fix
```

## Current Implementation Status

### Completed
- [x] Project initialization with Expo + TypeScript
- [x] CalDAV service with event fetching and connection testing
- [x] Gitea service with issue CRUD operations
- [x] Secure credential storage (keyring service)
- [x] React Context state management
- [x] Platform-aware navigation (sidebar for web, tabs for mobile)
- [x] Iconify icon integration for web
- [x] All main screens implemented (Dashboard, Inbox, Events, Notes, Tasks, Issues, Settings)

### Completed (continued)
- [x] Dark/light mode theming (system/light/dark toggle, persisted via AsyncStorage, all screens themed)
- [x] Offline caching (AsyncStorage, events/issues/notes cached, loaded on startup, lastSync timestamp)
- [x] Linux AppImage packaging (Electron wrapper, amd64 + aarch64, CI workflow)

### Completed (continued 2)
- [x] Native icon support (@expo/vector-icons MaterialCommunityIcons for native, Iconify for web)
- [x] CalDAV notes sync (VJOURNAL fetch/create/update/delete, wired to NotesScreen)
- [x] Push notifications (expo-notifications for local event reminders, UnifiedPush client for de-googled Android)

### Completed (continued 3)
- [x] Widget support (Android home screen widget — see Android Widget Module section for full details)

### Completed (continued 4)
- [x] CalDAV VTODO task support (full CRUD via `fetchTasks`, `createTask`, `updateTask`, `deleteTask`)
- [x] TasksScreen with nested subtask tree, filter bar (all/active/completed), CRUD modal
- [x] Subtask hierarchy via `RELATED-TO;RELTYPE=PARENT` with expand/collapse
- [x] Tasks integrated into InboxScreen (replaces note-based heuristic) and DashboardScreen ("Tasks Due Soon" section)
- [x] Intelligent task quick input parser (Todoist-style: `!! task #tag tonight`)
- [x] Configurable time-of-day defaults in Settings (morning/afternoon/night/default hours)
- [x] Live parse preview showing extracted priority, tags, and due date as chips
- [x] Task cache with encrypted storage (`saveTasks`/`loadTasks`)

### Completed (continued 5)
- [x] Nextcloud Login Flow v2 (`src/services/nextcloud.ts`) — browser-based login, auto-fills CalDAV credentials
- [x] Auth method toggle in Settings (Manual / Nextcloud) with CORS warning on web
- [x] Calendar discovery via PROPFIND — returns `CalDavCalendar[]` with href, displayName, color, components
- [x] Calendar picker UI in Settings — color dots, display names, component badges (Events/Tasks/Notes), checkboxes, Select All/Deselect All
- [x] Calendar-aware fetching — events, tasks, notes fetch from selected calendars only
- [x] Selected calendars persisted to keyring and AppContext (`selectedCalendars`)
- [x] Default event/task calendar settings — user picks which calendar new events/tasks are saved to
- [x] Calendar color dots on EventsScreen and TasksScreen — each item shows a colored dot matching its source calendar
- [x] Events and tasks tagged with `calendarHref` during fetch for calendar identification

### Completed (continued 6)
- [x] Property/detail pages for all item types (tasks, events, notes, issues)
- [x] Shared property page components (`PropertyPageShared.tsx`): modal shell, header with edit toggle, read fields, section headers
- [x] EventPropertyPage — read-only detail view with date/time range, duration, location, description
- [x] NotePropertyPage — read/edit modes, full content display, tag chips, delete action
- [x] TaskPropertyPage — read/edit modes, status badge, priority, progress bar, subtask list with completion toggle, categories, full edit form
- [x] IssuePropertyPage — read/edit modes, state badge, labels, assignees, body, open/close toggle, Gitea comment fetching/posting, "Open in Browser" link
- [x] Gitea API extensions: `fetchComments`, `addComment`, `updateIssue` in `gitea.ts`
- [x] `GiteaComment` type added to `src/types/index.ts`
- [x] New icons: `pencil` (mdi:pencil), `comment` (mdi:comment-text-outline)

### Completed (continued 7)
- [x] Pomodoro timer (`src/components/PomodoroTimer.tsx`) — modal overlay with countdown, session tracking, work/short break/long break phases
- [x] Pomodoro settings in `cache.ts` (`PomodoroSettings` interface, `savePomodoroSettings`/`loadPomodoroSettings`, defaults: 25min work, 5min short break, 15min long break, 4 sessions)
- [x] Pomodoro settings UI in SettingsScreen — configurable work/break durations and sessions until long break
- [x] Play button on TaskPropertyPage — starts Pomodoro timer, logs completed sessions to task description field (persisted via CalDAV)
- [x] Play button on IssuePropertyPage — starts Pomodoro timer (no session logging for issues)
- [x] New icons: `play`, `pause`, `stop`, `timer`

### Completed (continued 8 — Pomodoro enhancements)
- [x] Pomodoro state lifted to shared context (`src/store/PomodoroContext.tsx`) — timer logic extracted from component, exposed via `usePomodoro()` hook
- [x] `PomodoroProvider` wraps app in `App.tsx`; `PomodoroTimer` and `PomodoroMiniView` rendered at root level (above navigator)
- [x] `PomodoroTimer.tsx` refactored to presentational — reads all state from context, no local timer logic
- [x] TaskPropertyPage and IssuePropertyPage use context: play button calls `pomodoro.start(title, onSessionComplete?)` instead of local modal state
- [x] Desktop floating mini view (`src/components/PomodoroMiniView.tsx`) — fixed-position overlay (bottom-right, 220px), visible on web/macOS when timer active and modal hidden; shows phase color bar, task name, countdown (28px), phase label, play/pause + stop + expand controls
- [x] Android foreground service native module (`modules/pomodoro-service/`) — Expo module following `unified-push` pattern
- [x] `PomodoroForegroundService.kt` — persistent notification with Chronometer countdown, task name, Play/Pause + Stop action buttons, `CountDownTimer` for native-side countdown
- [x] `PomodoroAlarmReceiver.kt` — BroadcastReceiver for `AlarmManager.setExactAndAllowWhileIdle()` backup alarms
- [x] `PomodoroEventBus.kt` — singleton event bridge between Service/Receiver and Expo Module
- [x] `PomodoroServiceModule.kt` — Expo Module with `startTimer`/`pauseTimer`/`resumeTimer`/`stopTimer`/`canScheduleExactAlarms` functions + `onTick`/`onPhaseEnd`/`onAction`/`onAlarmFired` events
- [x] TypeScript wrapper (`modules/pomodoro-service/index.ts`) with `Platform.OS` guard, typed event listeners
- [x] Android permissions: `FOREGROUND_SERVICE`, `FOREGROUND_SERVICE_SPECIAL_USE`, `USE_EXACT_ALARM`, `SCHEDULE_EXACT_ALARM`, `POST_NOTIFICATIONS`
- [x] PomodoroContext integrates native module on Android: mirrors start/pause/resume/stop to native, resyncs JS state from native `onTick`/`onAction`/`onPhaseEnd` events

### Completed (continued 9 — visible screens)
- [x] Configurable visible screens — user can toggle which screens (Dashboard, Inbox, Events, Notes, Tasks, Issues, Views) appear in navigation
- [x] `ScreenName` type and `ALL_SCREENS` constant in `src/services/cache.ts`
- [x] `saveVisibleScreens`/`loadVisibleScreens` persistence via AsyncStorage
- [x] `visibleScreens` state in AppContext with `setVisibleScreens` setter (persists on change, loads on init)
- [x] SidebarNavigator and AppNavigator (mobile tabs) dynamically filter nav items by `visibleScreens` (Settings always shown)
- [x] Navigation section in SettingsScreen — checkbox toggles per screen with icons, at least one screen must remain visible

### Completed (continued 10 — Views)
- [x] Views screen (`src/screens/ViewsScreen.tsx`) — Kanban board + Covey's Four Quadrants (Eisenhower matrix)
- [x] Both tasks AND issues appear in views — tasks matched by `categories`, issues matched by `labels[].name`
- [x] Kanban view: horizontal scrollable columns, configurable column tags (persisted via `saveKanbanColumns`/`loadKanbanColumns`)
- [x] Covey's Four Quadrants view: 2x2 grid with Do (`#do`), Delay (`#delay`), Delegate (`#delegate`), Eliminate (`#eliminate`) tags
- [x] Assign modal: "+" button opens modal to assign tasks/issues to columns/quadrants — pick a target tag, tap an item to assign
- [x] Mutual exclusivity: within a view, an item can only have one tag (e.g. `#inprogress` OR `#backlog`, not both); cross-view tags are allowed (e.g. `#inprogress` + `#do`)
- [x] Sync back to source: task categories updated via CalDAV `updateTask()`, issue labels updated via Gitea `replaceIssueLabels()` (labels auto-created if missing via `createRepoLabel()`)
- [x] Compact item cards with type icon (task/bug), priority dot, due date, tap-to-open property page
- [x] View toggle chips (Kanban / Quadrants), filter icon opens column config modal for Kanban
- [x] New icon: `views` (mdi:view-column)
- [x] `'Views'` added to `ScreenName` type and `ALL_SCREENS` in `cache.ts`
- [x] Gitea API extensions: `createRepoLabel()`, `replaceIssueLabels()` in `gitea.ts`

### Completed (continued 11 — Inbox time)
- [x] Inbox total estimated time — shown at the end of the scrollable list after filtering
- [x] Events: duration calculated from start/end times; all-day events (midnight-to-midnight, >=24h) are excluded
- [x] Tasks: time estimated from `#Xm` or `#Xh` tags in `categories` (e.g. `#20m` = 20 minutes, `#2h` = 2 hours)
- [x] Issues: time estimated from `#Xm` or `#Xh` labels (same pattern as tasks)
- [x] Items without time information are excluded from the total
- [x] Formatted as "Xh Ym" (e.g. "3h 45m"), updates reactively when filters change

### Completed (continued 12 — bug fixes & UX)
- [x] **Bug**: CalDAV status dot and calendar selector hidden on launch — fixed `loadSettings()` to restore `availableCalendars` from keyring and set `caldavStatus = 'success'` when all credentials exist
- [x] **CalDAV calendar refresh button** — always-visible refresh icon button next to "Save Selection" in the calendar picker section (not only in the empty-state branch)
- [x] **Keyboard avoidance** — `PropertyPageShared.tsx` wraps modal in `KeyboardAvoidingView behavior="padding"` (works on both iOS and Android without the modal flying too high); `minHeight` raised from 50% to 70%
- [x] **Kanban quick tags in TaskPropertyPage** — chips for kanban column tags shown below Categories field; defaults to `['backlog', 'planned', 'inprogress', 'done']` when no columns saved
- [x] **Unified refresh hook** (`src/hooks/useSyncAll.ts`) — all screen refresh buttons use one hook; guards against clearing state on transient errors; updates Android widget with fresh data

### Completed (continued 13 — dashboard stats)
- [x] Local daily activity stats stored in AsyncStorage (`@cache/daily_stats`) as `Record<'YYYY-MM-DD', DailyStats>`
- [x] `DailyStats`: `{ tasksCompleted, pomodoroSessions, issuesClosed }` — incremented at the moment each event occurs
- [x] `incrementStat(stat)` called in: `TasksScreen.toggleCompletion`, `TasksScreen.saveTask`, `TaskPropertyPage.onSave` (task completions), `TaskPropertyPage.handlePomodoroSession` (pomodoro), `IssuesScreen.onStateToggle` (issue close)
- [x] Dashboard "Last 7 Days" stats card — 3 tiles (tasks done, pomodoros, issues closed), delta arrows (↑N / ↓N) vs previous 7 days; only shown when CalDAV or Gitea is configured

### Completed (continued 14 — Android widget overhaul)
- [x] **Title removed** — "Cross Dashboard" label removed from widget layout
- [x] **Size-aware rows** — `DashboardWidgetProvider` reads `OPTION_APPWIDGET_MIN_HEIGHT`, shows 1–3 rows per section via `setViewVisibility()`; `onAppWidgetOptionsChanged` handles live resize
- [x] **EVENTS and TASKS sections** — widget now shows upcoming events (formatted `MM/dd HH:mm title`) and pending tasks (with `⚠` overdue prefix) instead of just counts
- [x] **FAB** — circle `+` button (bottom-right, `@drawable/fab_background` oval) opens app via `crossdashboard://tasks?action=add` deep link; `TasksScreen` auto-focuses quick input on this param
- [x] **Deep link setup** — `app.json` `intentFilters` for `crossdashboard://` scheme; `AppNavigator.tsx` `LinkingOptions` for all screens; `TasksScreen` uses `useRoute` + `useRef` to focus quick input after 300ms
- [x] **WorkManager background sync** (`WidgetSyncWorker.kt`) — `CoroutineWorker` making CalDAV REPORT + Gitea REST calls; parses iCal line-by-line; updates SharedPreferences + broadcasts refresh
- [x] **Configurable sync interval** — Settings > Home Screen Widget (Android only); min 15 min, default 60 min; saved via `cache.saveWidgetSyncInterval()`; calls `DashboardWidget.scheduleSync()`
- [x] **Worker credentials** — `DashboardWidget.saveWorkerCredentials()` called from SettingsScreen after CalDAV save, Gitea save, and calendar selection save; stores all needed data in SharedPreferences
- [x] **Fixed "always not synced"** — `useSyncAll` now captures fresh events/tasks/issues from fetch results (not stale `state.*`) before calling `DashboardWidget.updateWidgetData()`; widget update uses `Platform.OS === 'android'` guard

---

## Agent Task Guidelines

### When Adding Features
1. Check if types exist in `src/types/` first
2. Create/update service in `src/services/` for API interactions
3. Build hooks in `src/hooks/` for data fetching logic
4. Create components in `src/components/` for reusable UI
5. Assemble screens in `src/screens/`

### When Handling Credentials
1. Always use `src/services/keyring.ts` wrapper
2. Prompt user for credentials via secure input
3. Store immediately to secure store
4. Never log or expose credentials

### When Building UI
1. Support dark/light mode from the start
2. Use responsive design for web/tablet/phone
3. Use `AppIcon` component with `Icons` constants for all icons
4. Follow platform-specific navigation patterns (sidebar for web, tabs for mobile)
5. Test on all target platforms before marking complete

### When Adding Icons
1. Import from `src/components/Icon.tsx`
2. Use existing icon from `Icons` constant, or add new ones following MDI naming
3. Always provide `size` and `color` props for consistency

### Error Handling
- Wrap all API calls in try-catch
- Provide user-friendly error messages
- Log errors for debugging (without sensitive data)
- Implement offline support where applicable

---

## Future Considerations
- Offline mode with local SQLite caching
- Multi-account support for CalDAV/Gitea
- Task recurrence (RRULE in VTODO)
- Nextcloud Android SSO (native Expo module using `Android-SingleSignOn` AIDL IPC — deferred, Login Flow v2 covers Android)
