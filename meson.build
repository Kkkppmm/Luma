project(
  'luma-builder',
  ['c', 'cpp'],
  version: '0.3.2',
  meson_version: '>= 1.0.0',
  default_options: [
    'warning_level=2',
    'werror=false',
    'c_std=c11',
    'cpp_std=c++17',
  ],
)

gnome = import('gnome')

dependency('gtk4', version: '>= 4.12')
dependency('libadwaita-1', version: '>= 1.4')
dependency('gtksourceview-5', version: '>= 5.0')
dependency('json-glib-1.0', version: '>= 1.6')
dependency('libsoup-3.0', version: '>= 3.0')
dependency('gio-2.0')

conf = configuration_data()
conf.set_quoted('APP_ID', 'io.github.kkkppmm.LumaBuilder')
conf.set_quoted('APP_NAME', 'Luma Builder')
conf.set_quoted('PACKAGE_VERSION', meson.project_version())
conf.set_quoted('GETTEXT_PACKAGE', meson.project_name())
conf.set_quoted('LOCALEDIR', join_paths(get_option('prefix'), get_option('localedir')))
conf.set_quoted('DATADIR', join_paths(get_option('prefix'), get_option('datadir')))
conf.set_quoted('DOCDIR', join_paths(get_option('prefix'), get_option('datadir'), 'doc', meson.project_name()))

configure_file(
  output: 'config.h',
  configuration: conf,
)

config_inc = include_directories('.')

subdir('data')
subdir('src')

gnome.post_install(
  glib_compile_schemas: true,
  gtk_update_icon_cache: true,
  update_desktop_database: true,
)
