fs = import('fs')

desktop_file = fs.copyfile(
  'io.github.kkkppmm.LumaBuilder.desktop.in',
  'io.github.kkkppmm.LumaBuilder.desktop',
  install: true,
  install_dir: join_paths(get_option('datadir'), 'applications'),
)

appstream_file = fs.copyfile(
  'io.github.kkkppmm.LumaBuilder.metainfo.xml.in',
  'io.github.kkkppmm.LumaBuilder.metainfo.xml',
  install: true,
  install_dir: join_paths(get_option('datadir'), 'metainfo'),
)

install_data(
  'io.github.kkkppmm.LumaBuilder.gschema.xml',
  install_dir: join_paths(get_option('datadir'), 'glib-2.0', 'schemas'),
)

install_data(
  'icons/hicolor/scalable/apps/io.github.kkkppmm.LumaBuilder.svg',
  install_dir: join_paths(get_option('datadir'), 'icons', 'hicolor', 'scalable', 'apps'),
)

install_data(
  '../docs/getting-started.md',
  '../docs/editor-guide.md',
  '../docs/sdk-guide.md',
  install_dir: join_paths(get_option('datadir'), 'doc', 'luma-builder'),
)

gnome.compile_schemas(depend_files: files('io.github.kkkppmm.LumaBuilder.gschema.xml'))
