#!/bin/bash
# create_deb.sh - Automates the packaging of TDE NordVPN GUI into a Debian .deb file

set -e

APP_NAME="tdenordgui"
VERSION="1.0.1"
ARCH="amd64"
MAINTAINER="NordVPN TQt3 Port <noreply@example.com>"
DESCRIPTION="A lightning-fast, native, and extremely lightweight graphical interface for the NordVPN Linux CLI, built natively for TDE using TQt3."

# Runtime deps must match Trinity TDE package names (libtqt3-mt, tdelibs14-trinity),
# not generic/invented names like tqt3, libtqt4, trinity-tdecore, trinity-tdeui.
DEPENDS="libc6, libgcc-s1, libstdc++6, libtqt3-mt, tdelibs14-trinity, libnotify4, libgrpc++1.51 | libgrpc++1, libprotobuf32 | libprotobuf23"

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

# To support execution if user moved it to root, we must find the binary properly.
echo "[*] Cleaning and Building Project..."
"$DIR/clean.sh"
"$DIR/build.sh"

cd "$DIR"

# 2. Prepare the Debian package structure
PKG_DIR="${APP_NAME}_${VERSION}_${ARCH}"
DEB_NAME="${PKG_DIR}.deb"

echo "[*] Creating Debian package structure..."
rm -rf "$PKG_DIR"
mkdir -p "$PKG_DIR/usr/bin"
mkdir -p "$PKG_DIR/usr/share/applications"
mkdir -p "$PKG_DIR/usr/share/icons/hicolor/48x48/apps"
mkdir -p "$PKG_DIR/DEBIAN"

# 3. Copy files to the structure
echo "[*] Copying binary and assets..."
BIN_PATH="gui/tqt/build/tdenordgui"
if [ ! -f "$BIN_PATH" ]; then
    BIN_PATH="build/tdenordgui" # If script is in gui/tqt
fi

if [ ! -f "$BIN_PATH" ]; then
    echo "[!] Error: Compiled binary not found!"
    exit 1
fi

cp "$BIN_PATH" "$PKG_DIR/usr/bin/$APP_NAME"
chmod 755 "$PKG_DIR/usr/bin/$APP_NAME"

# Create a temporary desktop entry
cat << 'EOF' > "$PKG_DIR/usr/share/applications/tdenordgui.desktop"
[Desktop Entry]
Name=NordVPN
Comment=Lightning-fast TQt3 GUI for NordVPN
Exec=tdenordgui
Icon=tdenordgui
Terminal=false
Type=Application
Categories=Network;Utility;
Keywords=vpn;nordvpn;network;security;
EOF
chmod 644 "$PKG_DIR/usr/share/applications/tdenordgui.desktop"

# Copy the logo for the desktop icon
ICON_PATH="gui/tqt/icons/icon_nordvpn.png"
if [ ! -f "$ICON_PATH" ]; then
    ICON_PATH="icons/icon_nordvpn.png"
fi
cp "$ICON_PATH" "$PKG_DIR/usr/share/icons/hicolor/48x48/apps/tdenordgui.png"
chmod 644 "$PKG_DIR/usr/share/icons/hicolor/48x48/apps/tdenordgui.png"

# 4. Generate the Debian control and maintainer scripts
echo "[*] Generating DEBIAN/control file and maintainer scripts..."
cat << EOF > "$PKG_DIR/DEBIAN/control"
Package: $APP_NAME
Version: $VERSION
Section: net
Priority: optional
Architecture: $ARCH
Depends: $DEPENDS
Maintainer: $MAINTAINER
Description: $DESCRIPTION
 TDE NordVPN GUI integrates cleanly with the Trinity Desktop Environment
 to provide a rapid, ultra-lightweight client for the NordVPN daemon using
 native TQt3 components and real-time gRPC communication. No Electron
 or Flutter overhead, pure C++ power.
EOF
chmod 644 "$PKG_DIR/DEBIAN/control"

cat << 'EOF' > "$PKG_DIR/DEBIAN/postinst"
#!/bin/sh
set -e
# Configuration automatique du dépôt APT pour les futures mises à jour
if [ -d /etc/apt/sources.list.d ]; then
    cat << 'REPEOF' > /etc/apt/sources.list.d/tdenordgui.list
# tdeNordgui APT Repository
deb [trusted=yes] https://seb3773.github.io/tdenordgui/ stable main
REPEOF
fi
exit 0
EOF
chmod 755 "$PKG_DIR/DEBIAN/postinst"

cat << 'EOF' > "$PKG_DIR/DEBIAN/postrm"
#!/bin/sh
set -e
if [ "$1" = "purge" ] || [ "$1" = "remove" ]; then
    rm -f /etc/apt/sources.list.d/tdenordgui.list
fi
exit 0
EOF
chmod 755 "$PKG_DIR/DEBIAN/postrm"

# 5. Build the .deb file
echo "[*] Building the .deb package..."
dpkg-deb --build "$PKG_DIR"

# 6. Cleanup the packaging directory
echo "[*] Cleaning up package directory..."
rm -rf "$PKG_DIR"

echo "[+] Done! Package generated: $DIR/$DEB_NAME"
