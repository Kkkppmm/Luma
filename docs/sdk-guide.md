# SDK & API Guide

GNOME App Builder can download Flatpak runtimes used to build and run
GNOME applications in a reproducible sandbox.

## What gets installed

From the **SDK Manager** you can install:

| Runtime | Purpose |
|---------|---------|
| `org.gnome.Platform` | Runtime libraries to *run* GNOME apps |
| `org.gnome.Sdk` | Headers, compilers, and tools to *build* apps |
| `org.freedesktop.Sdk.Extension.rust-stable` | Optional Rust toolchain |
| GNOME API docs (devhelp / online) | Reference for GTK, GLib, Adwaita |

## How download works

1. Open **SDK Manager** from the homescreen.
2. Choose a GNOME version (for example 46 or 47).
3. Click **Install** next to Platform or Sdk.
4. App Builder runs `flatpak install --user` non-interactively.

You need network access and the Flathub remote configured:

```bash
flatpak remote-add --if-not-exists --user flathub https://dl.flathub.org/repo/flathub.flatpakrepo
```

## Building with the SDK

After the Sdk is installed you can build a project with:

```bash
flatpak-builder --user --force-clean build-dir org.example.App.json
```

Or develop against system packages (`libgtk-4-dev`, `libadwaita-1-dev`)
without Flatpak — App Builder works with both workflows.
