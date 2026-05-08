# Cross-Dashboard Linux — Migration Plan

Native Linux port of Cross-Dashboard, targeting GTK3 + C++23.  
Reference implementation: **Android** (`native-android/`).  
Parity with macOS except where noted (no biometrics, no Nextcloud SSO).

**Session budget reminder:** at most 20 file edits per agent session.

---

## Decisions Log

| # | Topic | Decision |
|---|---|---|
| 1 | UI toolkit | GTK3 3.24 + libhandy-1 (not GTK4/libadwaita) — broader DE support (XFCE, KDE, GNOME) |
| 2 | C++ bindings | gtkmm-3.0 for GTK widgets; libhandy C API directly (no mm wrapper) |
| 3 | Markdown/LaTeX | cmark-gfm → HTML → `WebKitWebView` (webkit2gtk-4.0) + MathJax bundle for `$$...$$` |
| 4 | Min distro | Debian 13 / Ubuntu 22.04 LTS |
| 5 | Packaging | Flatpak (primary) + Debian `.deb` |
| 6 | Nextcloud auth | Login Flow v2 + Manual only (no SSO SDK on Linux) |
| 7 | Biometrics | Not implemented |
| 8 | Home widget | Not implemented (no cross-DE API) |
| 9 | Pomodoro tray | libappindicator3; GNOME 45+ requires AppIndicator Shell extension — document, do not work around |

---

## Tech Stack Reference

| Category | Library | Debian 13 package |
|---|---|---|
| UI | GTK3 3.24 (gtkmm-3.0) | `libgtkmm-3.0-dev` |
| Adaptive widgets | libhandy-1 | `libhandy-1-dev` |
| HTTP / CalDAV | libsoup-3.0 | `libsoup-3.0-dev` |
| JSON | nlohmann/json | `nlohmann-json3-dev` |
| SQLite | SQLite3 | `libsqlite3-dev` |
| Credentials | libsecret-1 | `libsecret-1-dev` |
| Markdown | cmark-gfm (+ extensions lib, linked by pkg-config) | `libcmark-gfm-dev`, `libcmark-gfm-extensions-dev` |
| Browser widget | webkit2gtk-4.0 | `libwebkit2gtk-4.0-dev` |
| Notifications | libnotify | `libnotify-dev` |
| System tray | libappindicator3 | `libappindicator3-dev` |
| Build | Meson + Ninja | `meson` |

---

## Adaptive UI Tiers

| Window width | `HdyLeaflet` state | Layout |
|---|---|---|
| < 720 px | Folded — hamburger in `HdyHeaderBar` to reveal sidebar | Single pane |
| 720 – 1100 px | Unfolded — sidebar always visible | List + Detail (`GtkPaned`) |
| > 1100 px | Unfolded + secondary `GtkPaned` split | Three-pane |

---

## Phase Status

| Phase | Title | Status |
|---|---|---|
| 1 | Project bootstrap + data layer | ✅ Done |
| 2 | Core UI & navigation | ✅ Done |
| 3 | Remaining screens | ✅ Done (verified `meson test` + `docker build -f docker/Dockerfile`) |
| 4 | Background sync & notifications | ✅ Done |
| 5 | Pomodoro timer | ✅ Done |
| 6 | Capture (Memos) screen | ✅ Done |
| 7 | Nav reorder, polish, packaging | ✅ Done |

Update phase status here (📋 Todo → 🔄 In Progress → ✅ Done) as work progresses.  
Also update `AGENTS.md` Implementation Status block to match.

---

## Phase 1 — Project Bootstrap & Data Layer

### Goal
Working Meson build; all data-layer classes compile and pass unit tests; no UI yet.

### Todos

#### Build system
- [x] `meson.build`, `subdir('src')` — C++23 project + Meson deps (SQLite, GLib/GIO, libsecret-1, libsoup-3.0, nlohmann-json)
- [x] `meson_options.txt` — `enable_flatpak`
- [x] `com.crossdashboard.app.desktop` / `metainfo.xml`, `flatpak/`, `debian/` packaging scaffold

#### Domain models
- [x] `src/domain/models.h` — Kotlin `Models.kt` port (`kAllScreens`, CalDAV/Gitea/Memos/Pomodoro; `InboxItem` → `std::variant`)

#### Database
- [x] `database.h/.cpp` — WAL, `user_version`, schema including `daily_stats`, `issue_comments`, attach tables
- [x] `event_dao` / `task_dao` / `note_dao` / `issue_dao` / `memo_dao` — headers + shared `daos.cpp` (`NULLS LAST` queries; subtree via `parent_uid`)
- [x] `stats_dao` — daily aggregate counters (`DailyStatsDao`)
- [ ] Dedicated `*_dao.cpp` & Gitea comment/asset tables (optional split from `daos.cpp`)

#### Credentials & preferences
- [x] `prefs.h/.cpp` — `SecretStore` (libsecret `secret_schema_new`) + `AppPreferences` (`GKeyFile` → `$XDG_CONFIG_HOME/crossdashboard/prefs.ini`) + `merged_app_preferences`; `CredentialKey` constants match Android/macOS


#### Network clients
- [x] `src/data/network/caldav_client.h/.cpp` — libsoup-3.0 `PROPFIND`/`REPORT`/`PUT`/`DELETE`/`HEAD`; Basic auth from `SecretStore`
- [x] `src/data/network/gitea_client.h/.cpp` — REST + multipart `attachment` upload for issue/comment assets
- [x] `src/data/network/memos_client.h/.cpp` — Bearer auth, Memos v1 (`std::optional` / tolerant JSON for proto3 omissions)
- [x] `src/data/network/nextcloud_login_flow.h/.cpp` — Login Flow v2 initiate + **`poll_blocking`** (2 s cadence via `g_usleep`, up to 5 min — same behaviour as Kotlin `poll()`)

#### Parsers
- [x] `src/data/parser/ical_parser.h/.cpp` — VEVENT / VTODO / VJOURNAL + serializers
- [x] `src/data/parser/task_input_parser.h/.cpp`

#### Repositories
- [x] `src/data/repository/repositories.h/.cpp` + `repo_utils.{h,cpp}` — event/task/note/issue/memos/stats repos (parity with Android `Repositories.kt` + `MemoRepository.kt` sync paging)

#### App container / entry
- [x] `src/app_container.h/.cpp`, `phase1_main.cpp`
- [x] `tests/parser_smoke.cpp` — `meson test` harness (parsers; no HTTP)

---

## Phase 2 — Core UI & Navigation

### Goal
App launches, shows sidebar + content area, Dashboard and Tasks screens functional.

### Todos

#### Application shell
- [x] `src/main.cpp` — `Gtk::Application`/`hdy_init` — `CdApplication::create()`
- [x] `src/application.h/.cpp` — `on_activate` / `HANDLES_OPEN`; `crossdashboard://` routing (tasks action=add, capture text=)
- [x] `AppContainer` created from `CdApplication::ensure_init()`, passed into `AppWindow`

#### App window & navigation
- [x] `src/ui/app_window.h/.cpp` — `Gtk::ApplicationWindow` + embedded `HdyLeaflet`; sidebar `GtkListBox`; `Gtk::Stack`; `HdyHeaderBar`; hamburger + back on narrow
- [x] `AppPreferences.visible_screens` drives sidebar/stack (Phase&nbsp;7 will react to reorder)
- [x] Folded leaflet → back button + menu toggle visibility via `notify::folded`
- [x] Dark/light from `merged_app_preferences` → `gtk-application-prefer-dark-theme`

#### ViewModels
- [x] `src/ui/app_viewmodel.h/.cpp` — sigc signals for capture text + “new task” (GObject-style parity deferred)

#### Core screens
- [x] `src/ui/screens/dashboard/dashboard_view.{h,cpp}` — weekly stats rollup, upcoming events, tasks due today, open-issue count from DB
- [x] `src/ui/screens/tasks/tasks_view.{h,cpp}` — nested indentation, checkbox completion, QuickInput (`TaskInputParser`) → `TaskRepository::create`

#### Components
- [x] `src/ui/components/property_panel.h` — optional placeholder; list/detail for Phase&nbsp;3 screens uses each view’s inline pane (`Gtk::Paned` / scroll) rather than this widget
- [x] `src/ui/components/markdown_view.{h,cpp}` — cmark-gfm HTML + WebKit + bundled MathJax (`data/mathjax/tex-chtml.js`, `PACKAGE_DATADIR`)

---

## Phase 3 — Remaining Screens

### Todos

- [x] `src/ui/screens/events/events_view.h/.cpp` — day/week/month filter (radio group); coloured calendar dot per row; markdown detail (`MarkdownView`)
- [x] `src/ui/screens/notes/notes_view.h/.cpp` — grid/list (`GtkFlowBox`/`GtkListBox`); `HdySearchBar` wrapping `GtkSearchEntry`; new/edit `GtkDialog` (VJOURNAL create/update via `NoteRepository`)
- [x] `src/ui/screens/issues/issues_view.h/.cpp` — open/closed filter; toolbar + **New issue**; Gitea fetch for comments + issue/comment attachments; comment composer + `GtkFileChooserNative` for comment attachments
- [x] `src/ui/screens/issues/attachment_row.h/.cpp` — paperclip icon + name/size; `gtk_show_uri_on_window()`
- [x] `src/ui/screens/inbox/inbox_view.h/.cpp` — unified events/tasks/issues; `#Nm` / `#Nh` estimate line
- [x] `src/ui/screens/views/views_view.h/.cpp` — Kanban columns + GTK drag-and-drop (`text/plain`); column layout persisted in `AppPreferences::kanban_columns_json`; Covey 2×2 grid
- [x] `src/ui/screens/settings/settings_view.h/.cpp` — grouped forms (`Gtk::Frame`): accounts (libsecret), appearance (theme), sync/notifications/Pomodoro, About (`CD_APP_VERSION`). **Not** `HdyPreferencesWindow` (full window type); Nextcloud Login Flow toggle deferred to a later pass / Phase 4 glue
- [x] `src/ui/components/read_markdown_field.h/.cpp` — small-caps label + `MarkdownView`
- [x] `data/mathjax/tex-chtml.js` — MathJax 3 `tex-chtml` bundle; `install_data` → `share/cross-dashboard/mathjax/`; `MarkdownView` uses `PACKAGE_DATADIR` or `~/.local/share/cross-dashboard/` fallback

### Phase 3 — completion

Phase 3 is **closed**: all items in the checklist above are implemented, the app links successfully on Debian 13–style systems when **both** cmark-gfm dev packages are installed (`libcmark-gfm-dev` pulls `libcmark-gfm`; pkg-config also requires **`-lcmark-gfm-extensions`**, so add `libcmark-gfm-extensions-dev`). `debian/control` and `docker/Dockerfile` list both.

**Build verification:** `meson compile -C build && meson test -C build` from `native-linux/`; CI-style smoke: `docker build -f docker/Dockerfile .` (uses **`-j 1`** so the compile stays within ~1 GB RAM Docker/Colima budgets).

**Out of scope for Phase 3** (later phases): Capture / Memos screen (**Phase 6**), `SyncScheduler` / notifications (**Phase 4**), Pomodoro (**Phase 5**), Settings navigation reorder + polish (**Phase 7**).

---

## Phase 4 — Background Sync & Notifications

### Todos

- [x] `src/background/sync_scheduler.h/.cpp` — `SyncScheduler`; `start(interval_seconds)` schedules `g_timeout_add_seconds`; `sync_once()` calls all repositories `sync()`; after each sync calls `NotificationScheduler::reschedule_all()`
- [x] On first launch, write `~/.config/systemd/user/crossdashboard-sync.service` + `.timer` unit (interval from preferences) so sync runs when app is closed; do not overwrite if already present
- [x] `src/background/notification_scheduler.h/.cpp` — `NotificationScheduler`; `reschedule_all()` cancels existing `g_timeout_add` timers and re-registers one per upcoming event alarm within the next 7 days; fires `notify_notification_show()` at alarm time
- [x] Stable alarm IDs: `std::abs(static_cast<int>(std::hash<std::string>{}(uid)) % 100000)`
- [x] `BootReceiver` equivalent: systemd `.service` unit with `After=graphical-session.target`; on start, runs `cross-dashboard --reschedule-alarms` CLI flag handled in `Application::on_command_line()`

---

## Phase 5 — Pomodoro Timer

### Todos

- [x] `src/ui/app_viewmodel.h/.cpp` — add `PomodoroState` observable property; `PomodoroViewModel` inner class with `g_timeout_add` 1 s tick
- [x] `src/ui/components/pomodoro_bar.h/.cpp` — `GtkRevealer` wrapping an `GtkBox` (phase label + countdown + pause/stop buttons); anchored at bottom-end of `AppWindow` via `GtkOverlay`; visible when timer active and modal not shown
- [x] `src/ui/components/pomodoro_modal.h/.cpp` — `GtkDialog` with full controls (work/break/long-break, session count, start/pause/stop/skip)
- [x] `src/background/pomodoro_status_item.h/.cpp` — `AppIndicator` tray icon via libappindicator3; icon label updated every second with remaining time; menu: Pause, Stop, Skip, Show Window; graceful no-op if `APP_INDICATOR_STATUS_PASSIVE` (indicator service absent)
- [x] Session logging: on session complete, append "🍅 Pomodoro: HH:MM–HH:MM" line to task description via `CalDavClient::update_task()`

---

## Phase 6 — Capture (Memos) Screen

### Todos

- [x] `src/ui/screens/memos/memos_view.h/.cpp` — list with state filter (`GtkToggleButton` row: Normal / Archived), tag filter chips (`GtkFlowBox` of `GtkToggleButton`), search, visibility badge icon per row, create flow
- [x] `src/ui/screens/memos/memo_detail_view.h/.cpp` — `MarkdownView` for body; attachment thumbnails; comment thread `GtkListBox`; toolbar: Extract Tasks, Create Event, Comment on Issue, Open URL, Copy Link
- [x] `src/ui/screens/memos/create_memo_dialog.h/.cpp` — `GtkDialog` with `GtkTextView` body, visibility combo, `GtkFileChooserNative` for attachments
- [x] `src/ui/screens/memos/extract_tasks_dialog.h/.cpp` — shows parsed `- [ ] …` lines as checkboxes; confirm creates CalDAV tasks
- [x] `src/ui/screens/memos/create_event_from_memo_dialog.h/.cpp`
- [x] `src/ui/screens/memos/comment_on_issue_dialog.h/.cpp` — repo picker (`GtkComboBoxText` from `GITEA_REPOS` credential) → issue picker (`GtkListBox` from local DB); no free-form input
- [x] `src/ui/components/memo_auth_image.h/.cpp` — auth `GET` with `Authorization: Bearer` header → `GdkPixbuf` → `GtkImage`; 160 px thumbnail; tap opens URL via `gtk_show_uri()`
- [x] **Proto3 JSON nullability:** Memos JSON parsing uses optional extraction helpers + `value_or(default)` for omitted proto3 fields
- [x] **Attachment URL:** construct as `{host}/file/{att.name}/{att.filename}` — `att.name` already contains `attachments/` prefix; do not strip
- [x] **Share target:** `.desktop` `MimeType=text/plain;text/uri-list;`; `%u`/file-open paths route to capture prefill in `Application::on_open()`
- [x] **URL scheme capture cold-start:** `AppViewModel::trigger_capture(text)` handled from both `MemosView::signal_map()` (cold start) and capture signal subscription (warm)

---

## Phase 7 — Nav Reorder, Polish & Packaging

### Todos

#### Navigation reorder
- [x] `AppPreferences.visible_screens` — ordered comma-separated string; `kAllScreens` matches Android `ALL_SCREENS` order; `Settings` pinned last in prefs merge
- [x] Settings → Navigation: ↑/↓ reorder + Hide + hidden “Restore” rows (↑/↓ GTK3-friendly alternative to list-box DnD)
- [x] `AppWindow` sidebar + stack rebuild from prefs (`schedule_rebuild_navigation` idle callback)

#### Accessibility
- [x] Sidebar `GtkListBoxRow`: `atk_object_set_name()` (“Screen: …”); header bar buttons: tooltips + ATK names; nav editor buttons: tooltip + ATK
- [ ] Full Accerciser pass (manual)

#### Polish
- [x] `data/icons/scalable/apps/*.svg` — hicolor scalable app icon; `install_data()`
- [x] CSS: `data/themes/gtk.css` + `gtk-dark.css` (`@define-color` tokens); loaded from `PACKAGE_DATADIR` or `~/.local/share/cross-dashboard/themes/`
- [x] Shortcuts on `AppWindow` (`key_press`): `Ctrl+N` new task, `Ctrl+R` sync once, `Ctrl+F` search (Tasks / Notes / Capture)

#### Flatpak
- [x] `flatpak/com.crossdashboard.app.yml` — `runtime-version: '46'`, `sdk: org.gnome.Sdk`, `type: dir` source (MathJax + app via Meson install); `cleanup` for static libs
- [ ] CI/optional: `flatpak-builder --user --install --force-clean build-flatpak flatpak/com.crossdashboard.app.yml`

#### Debian `.deb`
- [x] `debian/control` — present
- [x] `debian/rules` — `dh_auto_configure -- --prefix=/usr`
- [x] `debian/install` — binary + desktop + metainfo + `usr/share/cross-dashboard` + icon
- [x] `debian/changelog` — initial entry
- [ ] Optional: `dpkg-buildpackage -b -us -uc` on Debian host

#### Final
- [x] Update `AGENTS.md` Platform Strategy + Linux implementation blurb
- [x] This phase table in `MIGRATION.md`

---

## Known Gotchas

- **libhandy C API in a gtkmm project**: call `hdy_init()` before constructing any `Hdy*` widget. Include `<handy.h>` and link `libhandy-1`. Cast `HdyApplicationWindow*` to `GtkWidget*` for gtkmm interop via `Glib::wrap()`.
- **libsoup-3.0 async on GLib main loop**: use `soup_session_send_async()` with a `GCancellable`; deliver results back to UI thread via `g_idle_add()`. Never block the main loop.
- **WebKitWebView sandbox**: by default `WebKitWebView` blocks `file://` URIs. Load MathJax via `webkit_web_view_load_html()` with a `base_uri` set to `g_get_user_data_dir()` so relative JS paths resolve.
- **GTK drag-and-drop API changed in GTK 3.22+**: use `gtk_drag_source_set()` / `gtk_drag_dest_set()` (old DnD API) not the new GTK4-style `GtkDragSource`/`GtkDropTarget` GObjects — those are GTK4 only.
- **libappindicator3 on GNOME 45+**: `app_indicator_set_status(indicator, APP_INDICATOR_STATUS_ACTIVE)` silently does nothing if the indicator service is absent. Check `app_indicator_get_status()` after setting — if still `PASSIVE`, log a one-time warning and fall back to in-window bar only.
- **`GtkFileChooserNative` return value**: the `response-id` signal delivers `GTK_RESPONSE_ACCEPT` (not `GTK_RESPONSE_OK`) on confirmation. Use `gtk_file_chooser_native_get_filename()` (single file) or `gtk_file_chooser_get_filenames()` (multi).
- **Proto3 JSON / Memos DTOs**: same rule as macOS — all optional server fields must be `std::optional<T>`. Use a helper `json_get_opt<T>(j, "key")` to safely extract with a default.
- **`GITEA_REPOS` format**: Linux saves as JSON array (same as macOS). Always try `nlohmann::json::parse()` first; fall back to comma-split for forward compat with any Android-originated backup.
- **Attachment file URL**: `{host}/file/{att.name}/{att.filename}` where `att.name` is `attachments/{id}`. Never strip the `attachments/` prefix.
- **Screen name**: user-visible label is **"Capture"**; internal string key is also `"Capture"` (matching `ALL_SCREENS`). Never show "Memos" in the UI.
