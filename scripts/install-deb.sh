#!/usr/bin/env bash
# Helper to install a Luma .deb on Ubuntu/Debian when dpkg may be wedged
# by unrelated kernel/DKMS (VirtualBox) failures.
set -euo pipefail

DEB="${1:-}"
if [ -z "$DEB" ] || [ ! -f "$DEB" ]; then
  echo "Usage: $0 path/to/luma-builder_VERSION_amd64.deb" >&2
  exit 1
fi

echo "==> Checking for broken kernel/DKMS state (common VirtualBox issue)"
if dpkg -l | grep -qE '^i[UFH] +linux-(image|headers)'; then
  echo "Some linux packages look unconfigured. Attempting to clear VirtualBox DKMS blockers..."
  sudo apt-get remove -y virtualbox-dkms 2>/dev/null || true
  sudo dkms remove virtualbox/7.0.16 --all 2>/dev/null || true
  sudo dkms remove virtualbox/7.0.14 --all 2>/dev/null || true
fi

echo "==> Repairing dpkg / apt"
sudo dpkg --configure -a || true
sudo apt-get -f install -y || true

echo "==> Installing $DEB"
sudo apt-get install -y "./$DEB" 2>/dev/null || sudo apt-get install -y "$DEB"

echo "==> Done. Launch with: luma-builder"
