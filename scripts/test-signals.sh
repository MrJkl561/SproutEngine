#!/usr/bin/env bash
# Tests the editor's signals: runs editor/tests/signals without a window and
# checks that every way of connecting and sending signals works.
#
#   scripts/test-signals.sh [debug|release]   build the editor, then test
#   SPROUT_EDITOR=path/to/sprout_editor scripts/test-signals.sh   test that editor
set -u
source "$(dirname "$0")/_common.sh"

editor="${SPROUT_EDITOR:-}"
if [ -z "$editor" ]; then
    configure || exit 1
    cmake --build "$BUILD" --target sprout_editor --parallel "$JOBS" || exit 1
    editor="$EDITOR"
fi
project="$ROOT/editor/tests/signals"
output="$("$editor" --project "$project" --check-scripts 2>&1)" || { echo "$output"; exit 1; }
output="$("$editor" --project "$project" --play --headless --frames 120 2>&1)"
echo "$output" | grep -E "CHECK|PASSED|connect:|rror"
if echo "$output" | grep -q "SIGNALS TEST PASSED"; then
    echo "Signals test passed."
    exit 0
fi
echo "Signals test FAILED:"
echo "$output"
exit 1
