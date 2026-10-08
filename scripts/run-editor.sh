#!/usr/bin/env bash
# Builds the editor (if anything changed) and opens it.
#
#   scripts/run-editor.sh                    opens your last project
#   scripts/run-editor.sh ~/Games/MyGame     opens that project folder
#   scripts/run-editor.sh debug              a Debug build
set -e
source "$(dirname "$0")/_common.sh"
configure
cmake --build "$BUILD" --target sprout_editor --parallel "$JOBS"
exec "$EDITOR" "${REST[@]}"
