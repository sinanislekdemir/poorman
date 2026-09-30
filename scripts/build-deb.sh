#!/bin/bash
# Builds a .deb package for Poor Man's Catalog.
#
# Run on Debian/Ubuntu (or inside an Ubuntu 20.04 container) so the binary
# targets an old glibc. See .github/workflows/release.yml for the CI use.

set -euo pipefail

APP_NAME="PoorMansCatalog"
PKG_NAME="poormanscatalog"
ARCH="amd64"

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

if [ -n "${VERSION:-}" ]; then
    VERSION="${VERSION#v}"
else
    VERSION="$(git -c safe.directory="$ROOT_DIR" describe --tags --abbrev=0 2>/dev/null | sed 's/^v//' || true)"
    if [ -z "$VERSION" ]; then
        VERSION="$(sed -n 's/^Version:[[:space:]]*//p' package.conf 2>/dev/null | head -1)"
    fi
    VERSION="${VERSION:-0.0.0}"
fi

BUILD_DIR="build"
STAGE_DIR="deb-build"
STAGE="${STAGE_DIR}/${PKG_NAME}_${VERSION}_${ARCH}"
DEB="${PKG_NAME}_${VERSION}_${ARCH}.deb"

if [ "${INSTALL_DEPS:-0}" = "1" ]; then
    export DEBIAN_FRONTEND=noninteractive
    apt-get update
    apt-get install -y --no-install-recommends \
        build-essential \
        qt5-qmake \
        qtbase5-dev \
        qtbase5-dev-tools \
        qttools5-dev-tools \
        libqt5sql5-sqlite \
        dpkg-dev \
        git
fi

echo "==> Building ${APP_NAME} ${VERSION}"
rm -rf "$BUILD_DIR" "$STAGE_DIR" "$DEB"
mkdir -p "$BUILD_DIR"

qmake -o "$BUILD_DIR/Makefile" PoorMansCatalog.pro
make -C "$BUILD_DIR" -j"$(nproc)"

if [ ! -f "$BUILD_DIR/$APP_NAME" ]; then
    echo "Build failed: $BUILD_DIR/$APP_NAME not found" >&2
    exit 1
fi

echo "==> Assembling package"
mkdir -p "$STAGE/DEBIAN" \
    "$STAGE/usr/bin" \
    "$STAGE/usr/share/applications" \
    "$STAGE/usr/share/icons/hicolor/256x256/apps" \
    "$STAGE/usr/share/doc/$PKG_NAME"

install -m0755 "$BUILD_DIR/$APP_NAME" "$STAGE/usr/bin/$PKG_NAME"
install -m0644 additional/PoorMansCatalog.desktop \
    "$STAGE/usr/share/applications/${PKG_NAME}.desktop"
sed -i 's|^Exec=.*|Exec=poormanscatalog|' \
    "$STAGE/usr/share/applications/${PKG_NAME}.desktop"
install -m0644 additional/icon.png \
    "$STAGE/usr/share/icons/hicolor/256x256/apps/${APP_NAME}.png"
install -m0644 LICENSE "$STAGE/usr/share/doc/$PKG_NAME/copyright"
gzip -9c README.md > "$STAGE/usr/share/doc/$PKG_NAME/README.md.gz"
chmod 0644 "$STAGE/usr/share/doc/$PKG_NAME/README.md.gz"

cat > "$STAGE/DEBIAN/postinst" <<'EOF'
#!/bin/sh
set -e
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database -q || true
fi
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    gtk-update-icon-cache -q -t -f /usr/share/icons/hicolor 2>/dev/null || true
fi
EOF

cat > "$STAGE/DEBIAN/postrm" <<'EOF'
#!/bin/sh
set -e
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database -q || true
fi
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    gtk-update-icon-cache -q -t -f /usr/share/icons/hicolor 2>/dev/null || true
fi
EOF
chmod 0755 "$STAGE/DEBIAN/postinst" "$STAGE/DEBIAN/postrm"

INSTALLED_SIZE="$(du -sk "$STAGE" | cut -f1)"
cat > "$STAGE/DEBIAN/control" <<EOF
Package: $PKG_NAME
Version: $VERSION
Section: utils
Priority: optional
Architecture: $ARCH
Maintainer: Sinan Islekdemir <sinan@islekdemir.com>
Installed-Size: $INSTALLED_SIZE
Homepage: https://github.com/sinanislekdemir/poorman
Depends: libc6 (>= 2.29), libgcc-s1, libstdc++6, libqt5core5a (>= 5.12.2), libqt5gui5 (>= 5.12.2) | libqt5gui5-gles (>= 5.12.2), libqt5widgets5 (>= 5.12.2), libqt5sql5 (>= 5.12.2), libqt5sql5-sqlite (>= 5.12.2)
Description: Disk and path catalog creator
 Poor Man's Catalog creates searchable indexes (catalogs) of disks, external
 drives, DVDs and flash drives. Catalogs are stored in SQLite databases, so you
 can keep a separate database per disk and search through it later without
 plugging the disk back in.
EOF

dpkg-deb --root-owner-group --build "$STAGE" "$DEB"

echo "==> Created $DEB"
