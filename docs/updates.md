# Updates

Luma Builder can check GitHub for newer packages and install them on your PC.

## How it works

1. On startup (when GSettings `auto-check-updates` is enabled) or via **Check for Updates** (`Ctrl+U`), the app calls:

   `https://api.github.com/repos/Kkkppmm/Luma/releases/latest`

2. It compares the release tag to the running `PACKAGE_VERSION`.
3. It picks a package for your system:
   - Debian/Ubuntu-like → `.deb`
   - Fedora/RHEL-like → `.rpm`
   - otherwise → portable `.tar.gz`
4. If you choose **Install**, the package is downloaded to `~/.cache/luma-builder/updates/` and installed:
   - `.deb` → `pkexec apt-get install -y <file>`
   - `.rpm` → `pkexec dnf install -y <file>` (fallback `rpm -Uvh`)
   - `.tar.gz` → extract to `~/.local/opt/luma-builder` and symlink `~/.local/bin/luma-builder`

A polkit password prompt may appear for `.deb` / `.rpm` installs.

## Disable auto-check

```bash
gsettings set io.github.kkkppmm.LumaBuilder auto-check-updates false
```

## Publishing updates (maintainers)

Create a GitHub Release with assets named like:

- `luma-builder_VERSION_amd64.deb`
- `luma-builder-VERSION-*.x86_64.rpm`
- `luma-builder-VERSION-linux-amd64.tar.gz`

Clients on older versions will detect the new tag and offer install automatically.
