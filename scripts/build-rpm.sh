#!/bin/bash
# Builds an RPM package for Poor Man's Catalog.
#
# Run on Fedora/RHEL (or inside a matching container). BuildRequires from
# poormanscatalog.spec must be installed. See the README for the one-liner.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

VERSION="${VERSION:-$(sed -n 's/^Version:[[:space:]]*//p' poormanscatalog.spec | head -1)}"
SPEC="poormanscatalog.spec"
TOP_DIR="rpmbuild"
TARBALL="poorman-${VERSION}.tar.gz"

if [ "${INSTALL_DEPS:-0}" = "1" ]; then
    dnf install -y rpm-build gcc-c++ make qt5-qtbase-devel qt5-qttools-devel desktop-file-utils
fi

rm -rf "$TOP_DIR"
mkdir -p "$TOP_DIR"/{BUILD,BUILDROOT,RPMS,SOURCES,SPECS,SRPMS}

echo "==> Creating source tarball ${TARBALL}"
# Build from the working tree (tracked files) so local changes are included.
git ls-files -z | tar --null --files-from=- \
    --transform="s,^,poorman-${VERSION}/," \
    -czf "$TOP_DIR/SOURCES/${TARBALL}"

cp "$SPEC" "$TOP_DIR/SPECS/"

echo "==> Running rpmbuild"
rpmbuild --define "_topdir ${ROOT_DIR}/${TOP_DIR}" \
    --define "_sourcedir ${ROOT_DIR}/${TOP_DIR}/SOURCES" \
    -bb "$TOP_DIR/SPECS/${SPEC}"

echo "==> Built packages:"
find "$TOP_DIR/RPMS" -name '*.rpm' -print
