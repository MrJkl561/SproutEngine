#!/usr/bin/env bash
# Exports a game made with the editor, without opening the editor.
#
#   scripts/export.sh linux                          the demo project, as a Linux game
#   scripts/export.sh linux,windows,web              several platforms at once
#   scripts/export.sh linux ~/Games/MyGame           another project
#   scripts/export.sh linux ~/Games/MyGame ~/out     ...and where to put it
#
# Platforms: linux (on Linux), macos (on a Mac), windows (needs MinGW-w64:
# sudo apt install mingw-w64 / brew install mingw-w64), web (run
# scripts/setup-web.sh once first).
# The game goes in the project's exports/ folder unless you give one.
set -e
source "$(dirname "$0")/_common.sh"
TARGETS="${REST[0]}"
PROJECT="${REST[1]}"
OUT="${REST[2]}"
if [ -z "$TARGETS" ]; then
    echo "Which platforms? For example:  scripts/export.sh linux,web"
    exit 1
fi
if [ ! -x "$EDITOR" ]; then
    echo "== Building the editor first =="
    configure
    cmake --build "$BUILD" --target sprout_editor --parallel "$JOBS"
fi
args=()
if [ -n "$PROJECT" ]; then args+=(--project "$PROJECT"); fi
args+=(--export "$TARGETS")
if [ -n "$OUT" ]; then args+=("$OUT"); fi
exec "$EDITOR" "${args[@]}"
