#!/bin/bash
# Poor Man's Catalog Packaging Script
# Creates both AppImage and DEB packages

set -e

APP_NAME="PoorMansCatalog"
VERSION="1.0.6"
ARCH="x86_64"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Directories
BUILD_DIR="build"
APPDIR="AppDir"
DEB_BUILD_DIR="deb-build"

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}Poor Man's Catalog Package Builder${NC}"
echo -e "${GREEN}Version: $VERSION${NC}"
echo -e "${GREEN}========================================${NC}"

# Check dependencies
check_dependency() {
    if ! command -v $1 &> /dev/null; then
        echo -e "${RED}Error: $1 is not installed${NC}"
        echo "Please install it with: sudo apt install $2"
        exit 1
    fi
}

echo -e "${YELLOW}Checking dependencies...${NC}"
check_dependency qmake qt5-qmake
check_dependency make build-essential
check_dependency equivs-build equivs
check_dependency wget wget

# Clean previous builds
echo -e "${YELLOW}Cleaning previous builds...${NC}"
rm -rf "$BUILD_DIR" "$APPDIR" "$DEB_BUILD_DIR" *.AppImage *.deb
mkdir -p "$BUILD_DIR"

# Build the application
echo -e "${YELLOW}Building application...${NC}"
qmake -o "$BUILD_DIR/Makefile" PoorMansCatalog.pro
make -C "$BUILD_DIR" -j$(nproc)

if [ ! -f "$BUILD_DIR/$APP_NAME" ]; then
    echo -e "${RED}Build failed! Binary not found.${NC}"
    exit 1
fi

echo -e "${GREEN}Build successful!${NC}"

# ===============================================
# Create AppImage
# ===============================================
echo -e "${YELLOW}Creating AppImage...${NC}"

# Download linuxdeploy if not present
if [ ! -f linuxdeploy-x86_64.AppImage ]; then
    echo "Downloading linuxdeploy..."
    wget -q https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
    chmod +x linuxdeploy-x86_64.AppImage
fi

if [ ! -f linuxdeploy-plugin-qt-x86_64.AppImage ]; then
    echo "Downloading linuxdeploy-plugin-qt..."
    wget -q https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage
    chmod +x linuxdeploy-plugin-qt-x86_64.AppImage
fi

# Create AppDir structure
mkdir -p "$APPDIR/usr/bin"
mkdir -p "$APPDIR/usr/share/applications"
mkdir -p "$APPDIR/usr/share/icons/hicolor/256x256/apps"

# Copy binary
cp "$BUILD_DIR/$APP_NAME" "$APPDIR/usr/bin/"

# Copy desktop file
cp additional/PoorMansCatalog.desktop "$APPDIR/usr/share/applications/"

# Create or copy icon (using a default icon for now)
if [ -f additional/icon.png ]; then
    cp additional/icon.png "$APPDIR/usr/share/icons/hicolor/256x256/apps/$APP_NAME.png"
else
    # Create a simple placeholder icon if none exists
    echo -e "${YELLOW}Warning: No icon found, creating placeholder${NC}"
    convert -size 256x256 xc:blue -pointsize 48 -fill white -gravity center \
            -annotate +0+0 "PMC" "$APPDIR/usr/share/icons/hicolor/256x256/apps/$APP_NAME.png" 2>/dev/null || \
    echo "Note: ImageMagick not found, skipping icon creation"
fi

# Update desktop file to use correct icon
sed -i "s|Icon=.*|Icon=$APP_NAME|g" "$APPDIR/usr/share/applications/PoorMansCatalog.desktop"

# Run linuxdeploy
echo "Running linuxdeploy..."
export OUTPUT="${APP_NAME}-${VERSION}-${ARCH}.AppImage"
./linuxdeploy-x86_64.AppImage \
    --appdir "$APPDIR" \
    --plugin qt \
    --output appimage

if [ -f "$OUTPUT" ]; then
    echo -e "${GREEN}AppImage created successfully: $OUTPUT${NC}"
else
    echo -e "${RED}AppImage creation failed${NC}"
fi

# ===============================================
# Create DEB package
# ===============================================
echo -e "${YELLOW}Creating DEB package...${NC}"

# Update package.conf with build directory
sed -i "s|Files:.*|Files: $BUILD_DIR/$APP_NAME /usr/bin|" package.conf

# Build deb
equivs-build package.conf

# Restore original package.conf
git checkout package.conf 2>/dev/null || sed -i "s|Files:.*|Files: build/$APP_NAME /usr/bin|" package.conf

DEB_FILE=$(ls poormanscatalog_*.deb 2>/dev/null | head -1)
if [ -f "$DEB_FILE" ]; then
    echo -e "${GREEN}DEB package created successfully: $DEB_FILE${NC}"
else
    echo -e "${RED}DEB package creation failed${NC}"
fi

# ===============================================
# Summary
# ===============================================
echo ""
echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}Packaging Complete!${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""
echo "Created files:"
[ -f "$OUTPUT" ] && echo -e "  ${GREEN}✓${NC} AppImage: $OUTPUT"
[ -f "$DEB_FILE" ] && echo -e "  ${GREEN}✓${NC} DEB: $DEB_FILE"
echo ""
echo "Installation instructions:"
echo ""
echo "AppImage:"
echo "  chmod +x $OUTPUT"
echo "  ./$OUTPUT"
echo ""
echo "DEB:"
echo "  sudo dpkg -i $DEB_FILE"
echo "  sudo apt install -f  # Fix dependencies if needed"
echo ""
echo -e "${YELLOW}Note: You may want to clean up build files with:${NC}"
echo "  rm -rf $BUILD_DIR $APPDIR linuxdeploy*.AppImage"
