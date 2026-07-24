# Getting Started with GNOME App Builder

Welcome to **GNOME App Builder** — a lightweight IDE for creating GTK4 and
libadwaita applications in C and C++.

## Homescreen

When you launch the app you land on the homescreen:

- **New Project** — create a GTK4 / libadwaita template project
- **Recent Projects** — reopen projects you worked on before
- **SDK Manager** — download GNOME Platform and SDK runtimes
- **Help** — quick tips and keyboard shortcuts
- **Docs** — built-in documentation

## Creating a project

1. Click **New Project** on the homescreen.
2. Enter a project name and choose a parent folder.
3. Pick a template (GTK4 Application or GTK4 + libadwaita).
4. Click **Create**. The editor opens with your new project tree.

## Opening an existing project

Use **Open Project…** or click a card under **Recent Projects**.
The project root should contain a `meson.build` or `CMakeLists.txt`
(or any folder of source files).

## Next steps

- Browse the file tree on the left of the editor.
- Use the toolbar Format actions to tidy code.
- Install SDKs from the SDK Manager before packaging with Flatpak.
