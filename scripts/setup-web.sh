#!/usr/bin/env bash
# Gets Emscripten (emsdk) ready for web exports. Run it once.
#
#   scripts/setup-web.sh                   uses (or downloads) ~/emsdk
#   scripts/setup-web.sh ~/tools/emsdk     an emsdk folder you already have
set -e
EMSDK_DIR="${1:-$HOME/emsdk}"
if [ ! -f "$EMSDK_DIR/emsdk.py" ]; then
    if ! command -v git >/dev/null 2>&1; then
        echo "emsdk isn't in $EMSDK_DIR and Git isn't installed to download it."
        exit 1
    fi
    echo "== Downloading emsdk to $EMSDK_DIR =="
    git clone https://github.com/emscripten-core/emsdk.git "$EMSDK_DIR"
fi
cd "$EMSDK_DIR"
echo "== emsdk install latest (about 1 GB the first time) =="
./emsdk install latest
echo "== emsdk activate latest =="
./emsdk activate latest
echo
echo "Emscripten is ready in $EMSDK_DIR."
echo "Export the web version with:  scripts/export.sh web"
echo "(The editor finds emsdk in your home folder, Desktop or Documents by itself;"
echo " anywhere else, pick it with \"Choose...\" in its Export window.)"
