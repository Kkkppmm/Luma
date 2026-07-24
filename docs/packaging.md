# Packaging & releases

Build Linux packages for Luma Builder:

```bash
./scripts/build-packages.sh
# or
VERSION=0.3.1 ./scripts/build-packages.sh
```

## Artifacts (`dist/`)

| File | Distro / use |
|------|----------------|
| `luma-builder_VERSION_amd64.deb` | Debian, Ubuntu, Pop!_OS, Linux Mint, elementary |
| `luma-builder-VERSION-*.x86_64.rpm` | Fedora, RHEL, Alma, Rocky, openSUSE (via alien) |
| `luma-builder-VERSION-linux-amd64.tar.gz` | Any Linux x86_64 (portable) |
| `SHA256SUMS` | Checksums |

## Install

**Debian / Ubuntu**

```bash
sudo apt install ./luma-builder_0.1.0_amd64.deb
# or
sudo dpkg -i luma-builder_0.1.0_amd64.deb
sudo apt-get install -f   # if deps missing
```

**Fedora / RHEL-like**

```bash
sudo dnf install ./luma-builder-0.1.0-2.x86_64.rpm
```

**Portable tarball**

```bash
tar -xzf luma-builder-0.1.0-linux-amd64.tar.gz
cd luma-builder-0.1.0-linux-amd64
./run-luma-builder.sh
```

Runtime libraries still need to be installed from your distro (GTK4, libadwaita, GtkSourceView 5, etc.).

## Troubleshooting (Ubuntu)

### `InstallArchives() failed` / `dkms` / VirtualBox errors

If install fails with messages about **VirtualBox DKMS**, `linux-headers-*-generic`, or `linux-image-*-generic`, that is **not** a Luma Builder package bug. Ubuntu is trying to finish a pending kernel upgrade; VirtualBox’s kernel module fails to build, so `dpkg` stays broken and other installs fail too.

Luma’s own libraries (e.g. `libgtksourceview-5-0`) usually unpack fine — the failure is in the kernel/DKMS postinst.

**Fix the broken `dpkg` state first:**

```bash
# See what is stuck
sudo dpkg --audit
dpkg -l | grep -E 'linux-(image|headers).*7\.0\.0|virtualbox'

# Fastest unblock: stop VirtualBox DKMS from failing configure
sudo apt-get remove -y virtualbox-dkms || true
sudo dkms remove virtualbox/7.0.16 --all 2>/dev/null || true

# Finish pending package configuration
sudo dpkg --configure -a
sudo apt-get -f install -y
```

Then install Luma:

```bash
sudo apt-get install -y ./luma-builder_0.1.0_amd64.deb
```

**Optional:** reinstall / update VirtualBox afterward so it matches your new kernel:

```bash
sudo apt-get install -y virtualbox-dkms
# or install a newer VirtualBox from Oracle / your distro that supports the kernel
```

**Portable workaround** (no `dpkg` configure needed): use the `.tar.gz` from the release and run `./run-luma-builder.sh` after installing GTK runtime libs only:

```bash
sudo apt-get install -y libgtk-4-1 libadwaita-1-0 libgtksourceview-5-0 libjson-glib-1.0-0 libsoup-3.0-0
```
