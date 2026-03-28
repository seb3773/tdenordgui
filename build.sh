#!/bin/bash
set -e

echo "NordVPN TQt3 GUI - Build Script"
echo "================================="

# Navigate to the script's directory (assuming it's executed from root)
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$DIR/gui/tqt" 2>/dev/null || cd "$DIR" # Support running from root or from gui/tqt directly

echo "[1/3] Generating C headers from icons..."
if command -v xxd >/dev/null 2>&1; then
    mkdir -p src
    # Generate aggregate icon headers as expected by the C++ code
    (cd icons && for f in *.png; do xxd -i "$f"; done > ../src/settings_icons.h)
    python3 flags/generate_flags.py
    xxd -i icons/delete.png > src/delete_png.h
    echo "  -> Icons generated successfully."
else
    echo "  -> Error: 'xxd' tool not found. Cannot regenerate icons. Please install 'xxd' (vim-common)."
    exit 1
fi

echo "[2/3] Configuring CMake..."
mkdir -p build
cd build
cmake ..

echo "[3/3] Compiling Project..."
make -j$(nproc)

echo "[4/4] Stripping Binary..."
if command -v sstrip >/dev/null 2>&1; then
    echo "  -> Stripping binary (sstrip)..."
    sstrip tdenordgui
elif command -v strip >/dev/null 2>&1; then
    echo "  -> Stripping binary (strip)..."
    strip --strip-unneeded tdenordgui
fi

echo "================================="
echo "Build Successful!"
echo "Binary location: $(pwd)/tdenordgui"
