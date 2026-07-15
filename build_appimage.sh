#!/usr/bin/env bash
set -euo pipefail

SRC_ROOT="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SRC_ROOT/gui/tqt/build"
APPDIR="$BUILD_DIR/AppDir"

need_cmd() {
	command -v "$1" >/dev/null 2>&1 || {
		echo "error: missing required command: $1" >&2
		exit 1
	}
}

need_cmd cmake
need_cmd pkg-config
need_cmd strip
need_cmd sed
need_cmd awk
need_cmd wget
need_cmd cp
need_cmd chmod
need_cmd mkdir

# Make sure build dir exists
mkdir -p -- "$BUILD_DIR"

# Clean and Build the binary using the existing clean.sh and build.sh script
echo "info: building tdenordgui..."
"$SRC_ROOT/clean.sh"
"$SRC_ROOT/build.sh"

BIN_PATH="$BUILD_DIR/tdenordgui"
if test ! -x "$BIN_PATH"; then
	echo "error: missing built binary: $BIN_PATH" >&2
	exit 1
fi

# Clean and create AppDir structure
echo "info: preparing AppDir..."
rm -rf -- "$APPDIR"
mkdir -p -- \
	"$APPDIR/usr/bin" \
	"$APPDIR/usr/lib" \
	"$APPDIR/usr/share/applications" \
	"$APPDIR/usr/share/icons/hicolor/48x48/apps"

# Copy binary
cp -a "$BIN_PATH" "$APPDIR/usr/bin/tdenordgui"

# Strip staged binary
if command -v sstrip >/dev/null 2>&1; then
	echo "info: stripping staged binary with sstrip"
	sstrip "$APPDIR/usr/bin/tdenordgui" >/dev/null 2>&1 || true
else
	echo "info: using strip --strip-all"
	strip --strip-all "$APPDIR/usr/bin/tdenordgui" >/dev/null 2>&1 || true
fi

# Resolve and copy TQt3, TDE, gRPC, Protobuf and other custom libraries from ldd output
echo "info: copying library dependencies..."
while read -r libname libpath; do
	if [[ -n "$libpath" && -f "$libpath" ]]; then
		echo "  -> bundling: $libname ($libpath)"
		cp -L "$libpath" "$APPDIR/usr/lib/"
	else
		echo "  warning: library $libname not resolved to a valid file ($libpath)"
	fi
done < <(ldd "$BIN_PATH" | awk '/=>/ {print $1, $3}' | grep -E '^lib(tqt|tde|DCOP|art|grpc|protobuf|gpr|upb|address_sorting)')

# Copy icon
ICON_SRC="$SRC_ROOT/gui/tqt/icons/icon_nordvpn.png"
if [ ! -f "$ICON_SRC" ]; then
	ICON_SRC="$SRC_ROOT/icons/icon_nordvpn.png"
fi

if test -f "$ICON_SRC"; then
	cp -a "$ICON_SRC" "$APPDIR/tdenordgui.png"
	cp -a "$ICON_SRC" "$APPDIR/usr/share/icons/hicolor/48x48/apps/tdenordgui.png"
else
	echo "error: missing $ICON_SRC" >&2
	exit 1
fi

# Create Desktop entry at root of AppDir and usr/share/applications
cat > "$APPDIR/tdenordgui.desktop" <<EOF
[Desktop Entry]
Version=1.0
Name=NordVPN
Comment=Lightning-fast TQt3 GUI for NordVPN
Exec=tdenordgui
Icon=tdenordgui
Terminal=false
Type=Application
Categories=Network;Utility;
Keywords=vpn;nordvpn;network;security;
EOF
chmod 0644 "$APPDIR/tdenordgui.desktop"
cp -a "$APPDIR/tdenordgui.desktop" "$APPDIR/usr/share/applications/tdenordgui.desktop"

# Create AppRun entry script
cat > "$APPDIR/AppRun" <<'EOF'
#!/bin/sh
SELF=$(readlink -f "$0")
HERE=${SELF%/*}
export LD_LIBRARY_PATH="$HERE/usr/lib:$LD_LIBRARY_PATH"
exec "$HERE/usr/bin/tdenordgui" "$@"
EOF
chmod 0755 "$APPDIR/AppRun"

# Download appimagetool if not present
APPIMAGETOOL="$BUILD_DIR/appimagetool"
if [ ! -s "$APPIMAGETOOL" ]; then
	echo "info: downloading appimagetool..."
	wget -q --show-progress -O "$APPIMAGETOOL" "https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage"
	chmod +x "$APPIMAGETOOL"
fi

# Build AppImage using --appimage-extract-and-run to bypass FUSE requirements
OUT_APPIMAGE="$SRC_ROOT/tdenordgui-x86_64.AppImage"
rm -f -- "$OUT_APPIMAGE"

echo "info: generating AppImage..."
# Set ARCH environment variable so appimagetool knows what architecture we are packaging
export ARCH=x86_64
"$APPIMAGETOOL" --appimage-extract-and-run "$APPDIR" "$OUT_APPIMAGE"

echo "AppImage successfully built: $OUT_APPIMAGE"
exit 0
