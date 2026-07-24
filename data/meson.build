desktop_file = configure_file(
  input: 'org.gnome.AppBuilder.desktop.in',
  output: 'org.gnome.AppBuilder.desktop',
  copy: true,
  install: true,
  install_dir: join_paths(get_option('datadir'), 'applications'),
)

appstream_file = configure_file(
  input: 'org.gnome.AppBuilder.metainfo.xml.in',
  output: 'org.gnome.AppBuilder.metainfo.xml',
  copy: true,
  install: true,
  install_dir: join_paths(get_option('datadir'), 'metainfo'),
)

install_data(
  'org.gnome.AppBuilder.gschema.xml',
  install_dir: join_paths(get_option('datadir'), 'glib-2.0', 'schemas'),
)

install_data(
  'icons/hicolor/scalable/apps/org.gnome.AppBuilder.svg',
  install_dir: join_paths(get_option('datadir'), 'icons', 'hicolor', 'scalable', 'apps'),
)

install_data(
  '../docs/getting-started.md',
  '../docs/editor-guide.md',
  '../docs/sdk-guide.md',
  install_dir: join_paths(get_option('datadir'), 'doc', 'gnome-app-builder'),
)

# Compile schemas into the build directory for uninstalled runs
gnome.compile_schemas(depend_files: files('org.gnome.AppBuilder.gschema.xml'))
