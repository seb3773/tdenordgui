#!/bin/bash
set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$DIR/gui/tqt" 2>/dev/null || cd "$DIR" # Support running from root or from gui/tqt directly

echo "Cleaning NordVPN TQt3 GUI..."
rm -rf build/
rm -f src/settings_icons.h src/flags_icons.h src/delete_png.h
echo "Workspace sanitized."
