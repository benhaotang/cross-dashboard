# Cross-Dashboard - AI Agent Instructions

## Project Overview

Cross-Dashboard is a React Native application providing a unified web dashboard for:
- **CalDAV Events** - Calendar synchronization and event management
- **Notes** - Note-taking with CalDAV backend storage
- **Gitea Issues** - Issue tracking and management from Gitea repositories

### Target Platforms
- Android
- macOS (via React Native macOS or Expo)
- Web (via react-native-web)

### Tech Stack
- **Framework**: React Native with Expo (SDK 54)
- **Language**: TypeScript (strict mode)
- **Package Manager**: pnpm (strictly enforced)
- **UI**: React Native components with platform-specific adaptations
- **Icons**: Iconify (`@iconify/react`) - MDI icon set
- **Navigation**: React Navigation (bottom tabs for mobile, sidebar for web/desktop)
- **State Management**: React Context (`src/store/AppContext.tsx`)
- **Secure Storage**: expo-secure-store (keyring integration)

---

## Architecture

### Directory Structure
```
cross-dashboard/
├── src/
│   ├── components/     # Reusable UI components (Icon.tsx)
│   ├── navigation/     # Navigation configuration (AppNavigator, SidebarNavigator)
│   ├── screens/        # Screen components (Dashboard, Events, Notes, Issues, Settings)
│   ├── services/       # API clients (caldav.ts, gitea.ts, keyring.ts)
│   ├── hooks/          # Custom React hooks
│   ├── types/          # TypeScript type definitions (index.ts)
│   ├── utils/          # Helper utilities
│   └── store/          # State management (AppContext.tsx)
├── assets/             # Images, fonts, etc.
├── App.tsx             # Root component with providers
└── index.ts            # Entry point
```

### Security Guidelines
- **NEVER** hardcode credentials or API tokens
- All sensitive data (CalDAV passwords, Gitea tokens) MUST use `expo-secure-store`
- For web platform, use encrypted localStorage fallback with user-provided encryption key
- Validate all API responses before processing

### Key Services

#### CalDAV Service (`src/services/caldav.ts`)
- Authentication via stored credentials
- Event CRUD operations
- Calendar synchronization
- Notes storage (using VJOURNAL or custom collection)

#### Gitea Service (`src/services/gitea.ts`)
- Personal access token authentication
- Fetch issues from configured repositories
- Create/update issues
- Label and milestone management

#### Keyring Service (`src/services/keyring.ts`)
- Wrapper around expo-secure-store
- Platform-specific implementations for secure credential storage
- Methods: `setCredential`, `getCredential`, `deleteCredential`, `hasCredential`, `clearAllCredentials`
- Web fallback uses localStorage (less secure)

---

## UI Components

### Icon System (`src/components/Icon.tsx`)
- Uses `@iconify/react` for web platform
- Native fallback for Android/iOS (placeholder, can extend with react-native-vector-icons)
- Centralized icon names in `Icons` constant:
  ```typescript
  Icons.dashboard   // mdi:view-dashboard
  Icons.calendar    // mdi:calendar
  Icons.notes       // mdi:note-text
  Icons.issues      // mdi:bug
  Icons.settings    // mdi:cog
  Icons.refresh     // mdi:refresh
  Icons.add         // mdi:plus
  Icons.delete      // mdi:delete
  ```

### Navigation (`src/navigation/`)
- **AppNavigator.tsx**: Platform-aware navigation router
  - Web/macOS: Uses `SidebarNavigator`
  - Android/iOS: Uses bottom tab navigation
- **SidebarNavigator.tsx**: Sidebar navigation for desktop/web
  - Collapsible at <900px width
  - Shows icons and labels
  - Active state highlighting

### Screens (`src/screens/`)
| Screen | Description |
|--------|-------------|
| DashboardScreen | Overview with upcoming events and open issues |
| EventsScreen | CalDAV events with day/week/month filters |
| NotesScreen | Note management with create/edit/delete |
| IssuesScreen | Gitea issues with state filtering |
| SettingsScreen | Credential configuration for CalDAV and Gitea |

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
- Platform-specific code via `Platform.OS` or `.android.ts` / `.ios.ts` / `.web.ts` extensions
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

interface CalendarEvent {
  uid: string;
  summary: string;
  start: Date;
  end: Date;
  description?: string;
  location?: string;
}
```

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
pnpm ios        # iOS simulator (if on macOS)
pnpm web        # Web browser (sidebar navigation)

# Type checking
pnpm typecheck

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
- [x] All 5 main screens implemented

### In Progress
- [ ] Dark/light mode theming
- [ ] Offline caching

### Pending
- [ ] Native icon support (react-native-vector-icons)
- [ ] CalDAV notes sync (VJOURNAL)
- [ ] Push notifications
- [ ] Widget support

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
- Push notifications for calendar events
- Widget support (Android/macOS)
- Multi-account support for CalDAV/Gitea
