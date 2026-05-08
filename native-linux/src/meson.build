sqlite3_dep = dependency('sqlite3')
glib_dep = dependency('glib-2.0')
gio_dep = dependency('gio-2.0')
secret_dep = dependency('libsecret-1')
soup_dep = dependency('libsoup-3.0', required: true)
json_dep = dependency('nlohmann_json')
notify_dep = dependency('libnotify')
appindicator_dep = dependency('appindicator3-0.1', required: false)

gtkmm_dep = dependency('gtkmm-3.0')
handy_dep = dependency('libhandy-1')
webkit_dep = dependency('webkit2gtk-4.0', required: false)
if not webkit_dep.found()
  webkit_dep = dependency('webkit2gtk-4.1', required: true)
endif

cmark_dep = dependency('libcmark-gfm')

src_inc = include_directories('.', 'ui')

sources = files(
  'data/db/database.cpp',
  'data/db/daos.cpp',
  'data/db/stats_dao.cpp',
  'data/prefs/prefs.cpp',
  'data/network/http_soup.cpp',
  'data/network/caldav_client.cpp',
  'data/network/gitea_client.cpp',
  'data/network/memos_client.cpp',
  'data/network/nextcloud_login_flow.cpp',
  'data/parser/ical_parser.cpp',
  'data/parser/task_input_parser.cpp',
  'data/repository/repo_utils.cpp',
  'data/repository/repositories.cpp',
  'background/notification_scheduler.cpp',
  'background/pomodoro_status_item.cpp',
  'background/sync_scheduler.cpp',
  'app_container.cpp',
  'application.cpp',
  'main.cpp',
  'ui/app_window.cpp',
  'ui/app_viewmodel.cpp',
  'ui/components/markdown_view.cpp',
  'ui/components/pomodoro_bar.cpp',
  'ui/components/pomodoro_modal.cpp',
  'ui/screens/dashboard/dashboard_view.cpp',
  'ui/screens/tasks/tasks_view.cpp',
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
  'ui/components/attachment_row.cpp',
)

deps = [
  sqlite3_dep,
  glib_dep,
  gio_dep,
  secret_dep,
  soup_dep,
  json_dep,
  notify_dep,
  gtkmm_dep,
  handy_dep,
  webkit_dep,
  cmark_dep,
]
if appindicator_dep.found()
  deps += [appindicator_dep]
endif

executable(
  'cross-dashboard',
  sources,
  include_directories: src_inc,
  dependencies: deps,
  cpp_args: [
    app_version_define,
    package_datadir_arg,
    appindicator_dep.found() ? '-DCD_HAVE_APPINDICATOR=1' : '-DCD_HAVE_APPINDICATOR=0',
  ],
  install: true,
)

install_data(
  '../data/mathjax/tex-chtml.js',
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
