gtk_dep = dependency('gtk4')
adw_dep = dependency('libadwaita-1')
gsv_dep = dependency('gtksourceview-5')
json_dep = dependency('json-glib-1.0')
soup_dep = dependency('libsoup-3.0')

sources = files(
  'main.c',
  'application.c',
  'window.c',
  'secondary_window.c',
  'homescreen.c',
  'editor_view.c',
  'sdk_view.c',
  'help_view.c',
  'docs_view.c',
  'about_view.c',
  'project_manager.cpp',
  'project_manager_c.cpp',
  'sdk_manager.cpp',
  'sdk_manager_c.cpp',
  'code_formatter.cpp',
  'code_formatter_c.cpp',
  'build_runner.cpp',
  'build_runner_c.cpp',
  'updater.cpp',
  'updater_c.cpp',
)

gnome_app_builder = executable(
  'luma-builder',
  sources,
  dependencies: [gtk_dep, adw_dep, gsv_dep, json_dep, soup_dep],
  include_directories: config_inc,
  install: true,
)
