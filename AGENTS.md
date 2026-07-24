# AGENTS.md

## Cursor Cloud specific instructions

### Product

**GNOME App Builder** (`gnome-app-builder`) is a GTK4 / libadwaita desktop IDE written in **C + C++** (Meson). It scaffolds GNOME apps, edits sources with a file tree + format tools, and opens Help / Docs / SDK Manager / About as **separate windows**.

### Build & run (dev)

Standard commands are in `README.md`. Important Cloud/agent caveats:

- Prefer **`CC=gcc CXX=g++`** for Meson. The default `c++` may be Clang without a usable `libstdc++` link in this environment.
- Uninstalled runs need schemas:

```bash
CC=gcc CXX=g++ meson setup build --prefix=$HOME/.local
meson compile -C build
GSETTINGS_SCHEMA_DIR=build/data GDK_BACKEND=x11 ./build/src/gnome-app-builder
```

- Run from the **repo root** so `docs/*.md` resolve in the Docs window.
- Only one app instance runs (`GApplication`). A second launch activates the existing one and exits.
- Formatter unit check (no GUI): `bash scripts/test_formatter.sh`

### Services / windows

| Piece | How to open | Notes |
|-------|-------------|--------|
| Main homescreen + editor | App launch / New·Open project | Editor is an in-window nav page |
| Help | Homescreen **Help**, `F1`, or `app.help` | Own window + topic sidebar |
| Docs | Homescreen **Docs** or `app.docs` | Own window + doc sidebar |
| SDK Manager | Homescreen card or `app.sdk` | Own window; needs `flatpak` + network to install |
| About | Homescreen **About** or `app.about` | Own window |

Optional: Flatpak/Flathub for SDK installs. Not required to edit or scaffold projects.

### Lint / test

No ESLint. C/C++ warnings come from the compiler during `meson compile`. Use `scripts/test_formatter.sh` for the C++ formatter engine.
