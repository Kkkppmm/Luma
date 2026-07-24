# GNOME App Builder

Lightweight GTK4 / libadwaita IDE for creating GNOME applications in **C** and **C++**.

## Features

- Homescreen with **New Project**, **Recent Projects**, **Help**, and **Docs**
- **SDK Manager** to download GNOME Platform / Sdk Flatpak runtimes
- Editor with a **file tree**, syntax highlighting (GtkSourceView), and
  **format tools** (indent, trim, sort lines, toggle comments, format document)

## Dependencies

- Meson ≥ 1.0, Ninja, GCC/G++
- GTK 4, libadwaita 1, GtkSourceView 5
- json-glib, libsoup-3.0
- Optional: Flatpak (for SDK downloads)

On Ubuntu 24.04:

```bash
sudo apt install meson ninja-build build-essential pkg-config \
  libgtk-4-dev libadwaita-1-dev libgtksourceview-5-dev \
  libjson-glib-dev libsoup-3.0-dev gettext desktop-file-utils flatpak
```

## Build & run

```bash
meson setup build
meson compile -C build
./build/src/gnome-app-builder
```

For a local install (schemas + icons):

```bash
meson setup build --prefix=$HOME/.local
meson compile -C build
meson install -C build
gnome-app-builder
```

## Project layout

| Path | Role |
|------|------|
| `src/` | Application sources (C UI + C++ project/SDK/formatter engines) |
| `data/` | Desktop file, AppStream, GSettings schema, icon |
| `docs/` | Built-in documentation shown in the Docs view |

## License

MIT — see [LICENSE](LICENSE).
