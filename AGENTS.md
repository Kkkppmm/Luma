# AGENTS.md

## Cursor Cloud specific instructions

### Product

**Luma Builder** (`luma-builder`, app id `io.github.kkkppmm.LumaBuilder`) is a GTK4 / libadwaita desktop IDE in **C + C++** (Meson). Homescreen + editor live in the main window; Help / Docs / SDK Manager / About open as separate windows. Editor format tools are on the **right-click** context menu (and the header Tools menu).

### Build & run (dev)

See `README.md`. Cloud caveats:

- Prefer **`CC=gcc CXX=g++`** (default `c++`/Clang may fail to link `libstdc++` here).
- Uninstalled:

```bash
CC=gcc CXX=g++ meson setup build --prefix=$HOME/.local
meson compile -C build
GSETTINGS_SCHEMA_DIR=build/data GDK_BACKEND=x11 ./build/src/luma-builder
```

- Run from the **repo root** so `docs/*.md` resolve.
- One GApplication instance only.
- Formatter check: `bash scripts/test_formatter.sh`

### Windows

| Window | Open via |
|--------|----------|
| Home + Editor | App launch / New·Open project |
| Help | Menu, `F1`, or `app.help` |
| Docs | Menu or `app.docs` |
| SDK Manager | Home card, menu, or `app.sdk` |
| About | Menu or `app.about` |

Flatpak is optional (only for SDK installs).
