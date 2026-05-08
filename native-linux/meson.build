project(
  'cross-dashboard',
  'cpp',
  version: '1.0.0',
  default_options: ['cpp_std=c++23', 'warning_level=2'],
)

app_version_define = '-DCD_APP_VERSION="' + meson.project_version() + '"'
# Installed app resources (MathJax, etc.) — WebKit loads scripts relative to this directory.
package_datadir = join_paths(get_option('prefix'), get_option('datadir'), 'cross-dashboard')
package_datadir_arg = '-DPACKAGE_DATADIR="' + package_datadir + '"'

subdir('src')
