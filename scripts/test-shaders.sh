#!/usr/bin/env bash
# Tests custom shaders: plays editor/tests/shaders for a few frames (in a
# window; on Linux without a display, through xvfb-run) and checks the colours
# it draws: a parameter set in the scene, UV across a ColorRect, a texture
# tinted by a script and a parameter's default. Also checks that a broken
# shader's error names its line.
#
#   scripts/test-shaders.sh [debug|release]   build the editor, then test
#   SPROUT_EDITOR=path/to/sprout_editor scripts/test-shaders.sh   test that editor
set -u
source "$(dirname "$0")/_common.sh"

editor="${SPROUT_EDITOR:-}"
if [ -z "$editor" ]; then
    configure || exit 1
    cmake --build "$BUILD" --target sprout_editor --parallel "$JOBS" || exit 1
    editor="$EDITOR"
fi
project="$ROOT/editor/tests/shaders"
run=()
if [ "$(uname)" = "Linux" ] && [ -z "${DISPLAY:-}" ] && command -v xvfb-run >/dev/null; then run=(xvfb-run -a); fi
# (game pixels: blue | the UV gradient's corners and middle | green | magenta)
export SPROUT_TEST_PIXELS="40,45=0,0,255;81,1=0,0,0;158,88=255,255,0;120,45=128,128,0;200,45=0,255,0;280,45=255,0,255"
output="$("${run[@]}" "$editor" --project "$project" --play --frames 10 2>&1)"
echo "$output" | grep -E "PIXEL|Shader|rror"
if echo "$output" | grep -q "PIXELS OK" && echo "$output" | grep -q "broken.shader:4:"; then
    echo "Shaders test passed."
    exit 0
fi
echo "Shaders test FAILED:"
echo "$output"
exit 1
