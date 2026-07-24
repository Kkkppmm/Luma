# Packaging & releases

Build Linux packages for Luma Builder:

```bash
./scripts/build-packages.sh
# or
VERSION=0.1.0 ./scripts/build-packages.sh
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
