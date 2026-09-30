#!/bin/bash
# Builds a glibc-compatible AppImage for Poor Man's Catalog.
#
# Run this on (or inside a container of) the oldest still-supported Ubuntu LTS
# so the resulting AppImage only depends on an old glibc and runs on every
# supported distribution. See .github/workflows/release.yml for the CI use.

set -euo pipefail

APP_NAME="PoorMansCatalog"
ARCH="x86_64"

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

if [ -n "${VERSION:-}" ]; then
    VERSION="${VERSION#v}"
else
    VERSION="$(git describe --tags --abbrev=0 2>/dev/null | sed 's/^v//' || true)"
    VERSION="${VERSION:-0.0.0}"
fi

BUILD_DIR="build"
APPDIR="AppDir"
DESKTOP_FILE="additional/${APP_NAME}.desktop"
OUTPUT="${APP_NAME}-${VERSION}-${ARCH}.AppImage"

# Update information embedded in the AppImage so AppImageUpdate/zsync can find
# new releases. The matching .zsync file must be published next to the AppImage.
UPDATE_INFO="gh-releases-zsync|sinanislekdemir|poorman|latest|${APP_NAME}-*-${ARCH}.AppImage.zsync"

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
        wget \
        ca-certificates \
        file \
        patchelf \
        libfuse2 \
        desktop-file-utils
fi

echo "==> Building ${APP_NAME} ${VERSION}"
rm -rf "$BUILD_DIR" "$APPDIR" "$OUTPUT"
mkdir -p "$BUILD_DIR"

qmake -o "$BUILD_DIR/Makefile" PoorMansCatalog.pro
make -C "$BUILD_DIR" -j"$(nproc)"

if [ ! -f "$BUILD_DIR/$APP_NAME" ]; then
    echo "Build failed: $BUILD_DIR/$APP_NAME not found" >&2
    exit 1
fi

echo "==> Assembling AppDir"
mkdir -p "$APPDIR/usr/bin"
mkdir -p "$APPDIR/usr/share/applications"
mkdir -p "$APPDIR/usr/share/icons/hicolor/256x256/apps"

cp "$BUILD_DIR/$APP_NAME" "$APPDIR/usr/bin/"
cp "$DESKTOP_FILE" "$APPDIR/usr/share/applications/"

if [ -f additional/icon.png ]; then
    cp additional/icon.png "$APPDIR/usr/share/icons/hicolor/256x256/apps/${APP_NAME}.png"
fi

if [ ! -f linuxdeploy-x86_64.AppImage ]; then
    wget -q -O linuxdeploy-x86_64.AppImage \
        https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
fi

if [ ! -f linuxdeploy-plugin-qt-x86_64.AppImage ]; then
    wget -q -O linuxdeploy-plugin-qt-x86_64.AppImage \
        https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage
fi

chmod +x linuxdeploy-x86_64.AppImage linuxdeploy-plugin-qt-x86_64.AppImage

echo "==> Running linuxdeploy"
export OUTPUT
export APPIMAGE_EXTRACT_AND_RUN=1
export LDAI_UPDATE_INFORMATION="$UPDATE_INFO"
export UPDATE_INFORMATION="$UPDATE_INFO"
./linuxdeploy-x86_64.AppImage \
    --appdir "$APPDIR" \
    --plugin qt \
    --output appimage

if [ ! -f "$OUTPUT" ]; then
    echo "AppImage creation failed: $OUTPUT not found" >&2
    exit 1
fi

if [ ! -f "${OUTPUT}.zsync" ]; then
    echo "WARNING: ${OUTPUT}.zsync was not generated; AppImageUpdate will not work" >&2
fi

echo "==> Created $OUTPUT ($(du -h "$OUTPUT" | cut -f1))"
[ -f "${OUTPUT}.zsync" ] && echo "==> Created ${OUTPUT}.zsync"
