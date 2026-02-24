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
- **State Management**: React Context (`src/store/AppContext.tsx`) — events, notes, tasks, issues, selectedCalendars
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
│   └── store/          # State management (AppContext.tsx)
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
  ```

### Property Pages (`src/components/*PropertyPage.tsx`)
Full-screen modal overlays for viewing/editing item details. Built on shared components from `PropertyPageShared.tsx`:
- **PropertyPageModal** -- outer modal shell (slide animation, transparent, maxHeight 92%)
- **PropertyPageHeader** -- header with Close (left), title (center), pencil/cancel edit toggle (right)
- **ReadField** -- labeled read-only field row
- **SectionHeader** -- uppercase section divider
- **resolveCalendar()** -- resolves `calendarHref` to `{displayName, color}`

Property page components:
- **EventPropertyPage** -- read-only (no edit mode): date/time range, duration, location, description, UID
- **NotePropertyPage** -- read/edit toggle: tags as chips, full content, title/content/tags editing
- **TaskPropertyPage** -- read/edit toggle: status badge, priority, due date, progress bar, categories, subtask list with completion toggle, full edit form
- **IssuePropertyPage** -- read/edit toggle: state badge, labels, assignees, body, open/close toggle, comments section (fetched from Gitea API), add comment, "Open in Browser" link

### Navigation (`src/navigation/`)
- **AppNavigator.tsx**: Platform-aware navigation router
  - Web/macOS/Linux: Uses `SidebarNavigator`
  - Android: Uses bottom tab navigation
- **SidebarNavigator.tsx**: Sidebar navigation for desktop/web
  - Collapsible at <900px width
  - Shows icons and labels
  - Active state highlighting

### Screens (`src/screens/`)
| Screen | Description |
|--------|-------------|
| DashboardScreen | Overview with upcoming events, tasks due soon, and open issues |
| EventsScreen | CalDAV events with day/week/month filters, calendar color dots, tap-to-open read-only property page |
| NotesScreen | Note management with create/edit/delete, calendar-aware fetching, tap-to-open property page with read/edit modes |
| TasksScreen | VTODO task management with nested subtrees, quick input bar, CRUD modal, calendar color dots, tap-to-open property page with subtask list, read/edit modes |
| IssuesScreen | Gitea issues with state filtering, tap-to-open property page with comments, read/edit modes |
| InboxScreen | Unified inbox aggregating events, tasks, issues, and milestones |
| SettingsScreen | Nextcloud login / manual CalDAV, calendar picker with color dots & component badges, default event/task calendar selection, theme, task input defaults, notifications |

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
- [x] Widget support (Android home screen widget showing upcoming events count, open issues, next event, last sync)

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
