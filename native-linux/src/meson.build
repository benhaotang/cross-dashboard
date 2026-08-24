sqlite3_dep = dependency('sqlite3')
glib_dep = dependency('glib-2.0')
gio_dep = dependency('gio-2.0')
secret_dep = dependency('libsecret-1')
soup_dep = dependency('libsoup-3.0', required: true)
json_dep = dependency('nlohmann_json')
cairo_dep = dependency('cairo')
pangocairo_dep = dependency('pangocairo')
gdk_pixbuf_dep = dependency('gdk-pixbuf-2.0')
gdk_dep = dependency('gdk-3.0')
notify_dep = dependency('libnotify')
appindicator_dep = dependency('appindicator3-0.1', required: false)
appindicator_is_ayatana = false
if not appindicator_dep.found()
  appindicator_dep = dependency('ayatana-appindicator3-0.1', required: false)
  appindicator_is_ayatana = appindicator_dep.found()
endif
threads_dep = dependency('threads')
sigc_dep = dependency('sigc++-2.0')

gtkmm_dep = dependency('gtkmm-3.0')
handy_dep = dependency('libhandy-1')
webkit_dep = dependency('webkit2gtk-4.0', required: false)
if not webkit_dep.found()
  webkit_dep = dependency('webkit2gtk-4.1', required: true)
endif

cmark_dep = dependency('libcmark-gfm')

src_inc = include_directories('.', 'ui')

core_sources = files(
  'data/db/database.cpp',
  'data/db/daos.cpp',
  'data/db/stats_dao.cpp',
  'data/prefs/prefs.cpp',
  'data/network/http_soup.cpp',
  'data/network/caldav_client.cpp',
  'data/network/gitea_client.cpp',
  'data/network/memos_client.cpp',
  'data/network/karakeep_client.cpp',
  'data/network/nextcloud_login_flow.cpp',
  'data/parser/ical_parser.cpp',
  'data/parser/task_input_parser.cpp',
  'data/repository/repo_utils.cpp',
  'data/repository/operation_lock.cpp',
  'data/repository/repositories.cpp',
  'background/sync_runner.cpp',
  'background/background_definition.cpp',
  'background/background_content_builder.cpp',
  'background/background_renderer.cpp',
  'background/background_command.cpp',
  'background/background_manager.cpp',
  'background/pomodoro_session.cpp',
  'app_container.cpp',
)

gui_sources = core_sources + files(
  'background/pomodoro_status_item.cpp',
  'background/sync_scheduler.cpp',
  'application.cpp',
  'main.cpp',
  'ui/app_window.cpp',
  'ui/app_viewmodel.cpp',
  'ui/components/markdown_view.cpp',
  'ui/components/pomodoro_bar.cpp',
  'ui/components/pomodoro_modal.cpp',
  'ui/components/tag_flow.cpp',
  'ui/components/searchable_filter_menu.cpp',
  'ui/screens/dashboard/dashboard_view.cpp',
  'ui/screens/tasks/tasks_view.cpp',
  'ui/screens/tasks/task_edit_dialog.cpp',
  'ui/screens/tasks/quick_input_bar.cpp',
  'ui/components/read_markdown_field.cpp',
  'ui/components/memo_auth_image.cpp',
  'ui/screens/events/events_view.cpp',
  'ui/screens/notes/notes_view.cpp',
  'ui/screens/inbox/inbox_view.cpp',
  'ui/screens/issues/issues_view.cpp',
  'ui/screens/views/views_view.cpp',
  'ui/screens/settings/settings_view.cpp',
  'ui/screens/memos/memos_view.cpp',
  'ui/screens/memos/memo_detail_view.cpp',
  'ui/screens/memos/create_memo_dialog.cpp',
  'ui/screens/memos/extract_tasks_dialog.cpp',
  'ui/screens/memos/create_event_from_memo_dialog.cpp',
  'ui/screens/memos/comment_on_issue_dialog.cpp',
  'ui/screens/memos/save_to_karakeep_dialog.cpp',
  'ui/components/attachment_row.cpp',
)

core_deps = [
  sqlite3_dep,
  glib_dep,
  gio_dep,
  secret_dep,
  soup_dep,
  json_dep,
  cairo_dep,
  pangocairo_dep,
  gdk_pixbuf_dep,
  gdk_dep,
]

gui_deps = core_deps + [
  notify_dep,
  gtkmm_dep,
  handy_dep,
  webkit_dep,
  cmark_dep,
  sigc_dep,
]
if appindicator_dep.found()
  gui_deps += [appindicator_dep]
endif

gui_exe = executable(
  'cross-dashboard',
  gui_sources,
  include_directories: src_inc,
  dependencies: gui_deps,
  cpp_args: [
    app_version_define,
    package_datadir_arg,
    appindicator_dep.found() ? '-DCD_HAVE_APPINDICATOR=1' : '-DCD_HAVE_APPINDICATOR=0',
    appindicator_is_ayatana ? '-DCD_APPINDICATOR_AYATANA=1' : '-DCD_APPINDICATOR_AYATANA=0',
  ],
  install: true,
)

cli_exe = executable(
  'cross-dashboard-cli',
  core_sources + files('cli_main.cpp'),
  include_directories: src_inc,
  dependencies: core_deps + [threads_dep],
  install: true,
)

test('cli_help', cli_exe, args: ['--help'])

service_exe = executable(
  'cross-dashboard-service',
  core_sources + files(
    'background/notification_scheduler.cpp',
    'background_service_main.cpp',
  ),
  include_directories: src_inc,
  dependencies: core_deps + [notify_dep, threads_dep],
  install: true,
  install_dir: get_option('libexecdir'),
)

install_data(
  '../data/mathjax/tex-chtml.js',
  install_dir: get_option('datadir') / 'cross-dashboard' / 'mathjax',
)

install_subdir(
  '../data/mathjax/output',
  install_dir: get_option('datadir') / 'cross-dashboard' / 'mathjax',
)

parser_test_sources = files(
  '../tests/parser_smoke.cpp',
  'data/parser/ical_parser.cpp',
  'data/parser/task_input_parser.cpp',
)

parser_smoke_exe = executable(
  'parser_smoke_test',
  parser_test_sources,
  include_directories: include_directories('.', 'ui'),
  dependencies: glib_dep,
  install: false,
)

test('parser_smoke', parser_smoke_exe)

database_concurrency_exe = executable(
  'database_concurrency_test',
  files(
    '../tests/database_concurrency.cpp',
    'data/db/database.cpp',
    'data/db/daos.cpp',
    'data/repository/operation_lock.cpp',
  ),
  include_directories: include_directories('.', 'ui'),
  dependencies: [sqlite3_dep, glib_dep, threads_dep, json_dep],
  install: false,
)

test('database_concurrency', database_concurrency_exe)

cli_fuzzel_test_exe = executable(
  'cli_fuzzel_test',
  files(
    '../tests/cli_fuzzel.cpp',
    'data/db/database.cpp',
    'data/db/daos.cpp',
  ),
  include_directories: include_directories('.', 'ui'),
  dependencies: [sqlite3_dep, glib_dep, json_dep],
  install: false,
)
test('cli_fuzzel', cli_fuzzel_test_exe, args: [cli_exe.full_path()])

background_command_test_exe = executable(
  'background_command_test',
  files('../tests/background_command_test.cpp', 'background/background_command.cpp'),
  include_directories: src_inc,
  dependencies: [glib_dep],
  install: false,
)
test('background_command', background_command_test_exe)

background_renderer_test_exe = executable(
  'background_renderer_test',
  files('../tests/background_renderer_test.cpp', 'background/background_renderer.cpp'),
  include_directories: src_inc,
  dependencies: [cairo_dep, pangocairo_dep, gdk_pixbuf_dep, gdk_dep],
  install: false,
)
test('background_renderer', background_renderer_test_exe)

dbus_run_session = find_program('dbus-run-session')
service_pomodoro_test = find_program('../tests/service_pomodoro_test.sh')
test('service_pomodoro', dbus_run_session,
  args: [service_pomodoro_test.full_path(), service_exe.full_path(), cli_exe.full_path()],
  timeout: 30,
)

install_data(
  '../com.crossdashboard.app.desktop',
  install_dir: get_option('datadir') / 'applications',
)

install_data(
  '../data/icons/scalable/apps/com.crossdashboard.app.svg',
  install_dir: get_option('datadir') / 'icons' / 'hicolor' / 'scalable' / 'apps',
)

install_data(
  '../data/themes/gtk.css',
  '../data/themes/gtk-dark.css',
  install_dir: get_option('datadir') / 'cross-dashboard' / 'themes',
)

install_data(
  '../com.crossdashboard.app.metainfo.xml',
  install_dir: get_option('datadir') / 'metainfo',
)

install_man('../data/man/cross-dashboard-cli.1')

install_subdir(
  '../data/waybar',
  install_dir: get_option('datadir') / 'cross-dashboard',
)

service_config = configuration_data()
service_config.set('libexecdir', get_option('prefix') / get_option('libexecdir'))

systemd_dep = dependency('systemd', required: false)
systemd_user_unit_dir = get_option('prefix') / 'lib' / 'systemd' / 'user'
if systemd_dep.found()
  systemd_user_unit_dir = systemd_dep.get_variable(pkgconfig: 'systemduserunitdir')
endif

configure_file(
  input: '../data/systemd/crossdashboard.service.in',
  output: 'crossdashboard.service',
  configuration: service_config,
  install: true,
  install_dir: systemd_user_unit_dir,
)

configure_file(
  input: '../data/dbus/com.crossdashboard.Service.service.in',
  output: 'com.crossdashboard.Service.service',
  configuration: service_config,
  install: true,
  install_dir: get_option('datadir') / 'dbus-1' / 'services',
)
