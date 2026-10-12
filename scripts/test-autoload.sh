#!/usr/bin/env bash
# Tests autoloads: runs editor/tests/autoload without a window and checks that
# they're made before the scene, kept across a scene change, and found by scripts.
#
#   scripts/test-autoload.sh [debug|release]   build the editor, then test
#   SPROUT_EDITOR=path/to/sprout_editor scripts/test-autoload.sh   test that editor
set -u
source "$(dirname "$0")/_common.sh"

editor="${SPROUT_EDITOR:-}"
if [ -z "$editor" ]; then
    configure || exit 1
    cmake --build "$BUILD" --target sprout_editor --parallel "$JOBS" || exit 1
    editor="$EDITOR"
fi
project="$ROOT/editor/tests/autoload"
output="$("$editor" --project "$project" --check-scripts 2>&1)" || { echo "$output"; exit 1; }
output="$("$editor" --project "$project" --play --headless --frames 120 2>&1)"
echo "$output" | grep -E "CHECK|PASSED|FAILED|autoload|rror"
if echo "$output" | grep -q "AUTOLOAD TEST PASSED"; then
    echo "Autoload test passed."
    exit 0
fi
echo "Autoload test FAILED:"
echo "$output"
exit 1
