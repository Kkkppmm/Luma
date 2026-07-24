#!/usr/bin/env bash
# Build Luma Builder release packages: .deb, .tar.gz, and .rpm (via alien if available)
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VERSION="${VERSION:-0.3.3}"
ARCH="$(dpkg --print-architecture 2>/dev/null || uname -m)"
case "$ARCH" in
  x86_64) ARCH=amd64 ;;
esac

DIST="$ROOT/dist"
PREFIX_DIR="$DIST/prefix"
STAGE_DEB="$DIST/deb-root"
STAGE_TAR="$DIST/luma-builder-${VERSION}-linux-${ARCH}"

rm -rf "$DIST"
mkdir -p "$DIST" "$PREFIX_DIR"

echo "==> Configuring & installing to staging prefix"
cd "$ROOT"
rm -rf "$DIST/build"
CC="${CC:-gcc}" CXX="${CXX:-g++}" meson setup "$DIST/build" \
  --prefix=/usr \
  --buildtype=release \
  -Dwarning_level=1
meson compile -C "$DIST/build"
DESTDIR="$PREFIX_DIR" meson install -C "$DIST/build"

# Compile schemas into the staged tree
SCHEMA_DIR="$PREFIX_DIR/usr/share/glib-2.0/schemas"
if [ -d "$SCHEMA_DIR" ]; then
  glib-compile-schemas "$SCHEMA_DIR"
fi

echo "==> Building .deb"
mkdir -p "$STAGE_DEB/DEBIAN"
# Copy installed tree (DESTDIR layout is $PREFIX_DIR/usr/...)
cp -a "$PREFIX_DIR/usr" "$STAGE_DEB/"

# Runtime dependencies for Ubuntu/Debian
cat > "$STAGE_DEB/DEBIAN/control" <<EOF
Package: luma-builder
Version: ${VERSION}
Section: devel
Priority: optional
Architecture: ${ARCH}
Depends: libgtk-4-1, libadwaita-1-0, libgtksourceview-5-0, libjson-glib-1.0-0, libsoup-3.0-0, libc6
Maintainer: Luma Builder Contributors <noreply@users.noreply.github.com>
Homepage: https://github.com/Kkkppmm/Luma
Description: Luma Builder — lightweight GNOME/GTK4 IDE
 Luma Builder is a lightweight IDE for creating GTK4 and libadwaita
 applications in C and C++. It includes 30+ project templates, an editor
 with Build/Run (meson), format tools, Help/Docs/SDK windows, and a
 GitHub updater that can install new packages on the user's PC.
EOF

# postinst to compile schemas / update desktop db
cat > "$STAGE_DEB/DEBIAN/postinst" <<'EOF'
#!/bin/sh
set -e
if command -v glib-compile-schemas >/dev/null 2>&1; then
  glib-compile-schemas /usr/share/glib-2.0/schemas || true
fi
if command -v update-desktop-database >/dev/null 2>&1; then
  update-desktop-database -q /usr/share/applications || true
fi
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
  gtk-update-icon-cache -q /usr/share/icons/hicolor || true
fi
exit 0
EOF
chmod 755 "$STAGE_DEB/DEBIAN/postinst"

DEB_OUT="$DIST/luma-builder_${VERSION}_${ARCH}.deb"
dpkg-deb --build --root-owner-group "$STAGE_DEB" "$DEB_OUT"
echo "Built $DEB_OUT"

echo "==> Building portable .tar.gz"
mkdir -p "$STAGE_TAR"
cp -a "$PREFIX_DIR/usr" "$STAGE_TAR/"
cat > "$STAGE_TAR/README.txt" <<EOF
Luma Builder ${VERSION} (portable Linux ${ARCH})

Run (from this directory):
  ./usr/bin/luma-builder

Or install system-wide:
  sudo cp -a usr/* /usr/

Dependencies (Debian/Ubuntu package names):
  libgtk-4-1 libadwaita-1-0 libgtksourceview-5-0 libjson-glib-1.0-0 libsoup-3.0-0

Fedora/RHEL-ish:
  gtk4 libadwaita gtksourceview5 json-glib libsoup3

Arch:
  gtk4 libadwaita gtksourceview5 json-glib libsoup3
EOF
cat > "$STAGE_TAR/run-luma-builder.sh" <<'EOF'
#!/usr/bin/env bash
DIR="$(cd "$(dirname "$0")" && pwd)"
export GSETTINGS_SCHEMA_DIR="${DIR}/usr/share/glib-2.0/schemas${GSETTINGS_SCHEMA_DIR:+:$GSETTINGS_SCHEMA_DIR}"
export XDG_DATA_DIRS="${DIR}/usr/share:${XDG_DATA_DIRS:-/usr/local/share:/usr/share}"
exec "${DIR}/usr/bin/luma-builder" "$@"
EOF
chmod 755 "$STAGE_TAR/run-luma-builder.sh"
# Ensure schemas compiled in tarball too
glib-compile-schemas "$STAGE_TAR/usr/share/glib-2.0/schemas" || true

TAR_OUT="$DIST/luma-builder-${VERSION}-linux-${ARCH}.tar.gz"
tar -C "$DIST" -czf "$TAR_OUT" "$(basename "$STAGE_TAR")"
echo "Built $TAR_OUT"

echo "==> Building .rpm (alien) if available"
RPM_OUT=""
if command -v alien >/dev/null 2>&1; then
  (cd "$DIST" && alien -r --scripts "$(basename "$DEB_OUT")" 2>/dev/null) || true
  RPM_CAND=$(ls "$DIST"/luma-builder*.rpm 2>/dev/null | head -1 || true)
  if [ -n "$RPM_CAND" ]; then
    RPM_OUT="$RPM_CAND"
    echo "Built $RPM_OUT"
  fi
else
  echo "alien not installed — skipping .rpm (optional: sudo apt install alien)"
fi

# SHA256 sums
(cd "$DIST" && sha256sum luma-builder_*.deb luma-builder-*.tar.gz luma-builder*.rpm 2>/dev/null) \
  | tee "$DIST/SHA256SUMS" || true

echo
echo "Release artifacts in $DIST:"
ls -lh "$DIST"/*.{deb,tar.gz,rpm,SHA256SUMS} 2>/dev/null || ls -lh "$DIST"
