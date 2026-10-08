#!/usr/bin/env bash
# Builds the editor and puts a portable copy of it in one folder: the program
# plus everything it needs (its sources, the libraries, the templates). Move
# or copy that folder anywhere.
#
#   scripts/make-portable.sh                   into dist/SproutEditor
#   scripts/make-portable.sh path/to/folder    into that folder
set -e
source "$(dirname "$0")/_common.sh"
out="${REST[0]:-$ROOT/dist/SproutEditor}"
if [ -e "$out" ] && [ ! -e "$out/editor/src/script.hpp" ] && [ -n "$(ls -A "$out" 2>/dev/null)" ]; then
    echo "$out already has other files in it: pick an empty or new folder."
    exit 1
fi
configure
cmake --build "$BUILD" --target sprout_editor --parallel "$JOBS"
# (an earlier portable copy is replaced; checked above that it is one)
if [ -e "$out/editor/src/script.hpp" ]; then rm -rf "$out"; fi
cmake --install "$BUILD" --config "$CONFIG" --component sprout_editor --prefix "$out"
echo
echo "Done: a portable Sprout Editor is in $out"
echo "Start it with $out/sprout_editor"
