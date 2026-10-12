#!/usr/bin/env bash
# Tests the Script tab's extra checks (editor/src/script_lint.cpp): every line
# of editor/tests/lint/scripts/lint_me.cpp marked "// warn" gets a warning,
# and no other line does.
#
#   scripts/test-lint.sh [debug|release]   build the editor, then test
#   SPROUT_EDITOR=path/to/sprout_editor scripts/test-lint.sh   test that editor
set -u
source "$(dirname "$0")/_common.sh"

editor="${SPROUT_EDITOR:-}"
if [ -z "$editor" ]; then
    configure || exit 1
    cmake --build "$BUILD" --target sprout_editor --parallel "$JOBS" || exit 1
    editor="$EDITOR"
fi
project="$ROOT/editor/tests/lint"
file="$project/scripts/lint_me.cpp"
output="$("$editor" --project "$project" --check-file "$file" 2>&1)"
want="$(grep -n '// warn' "$file" | cut -d: -f1 | sort -n | tr '\n' ' ')"
got="$(echo "$output" | grep -E 'warning: (self|getNode|findNode|autoload)\(\)' | cut -d: -f2 | sort -n | uniq | tr '\n' ' ')"
echo "Lines that should warn: $want"
echo "Lines that warned:      $got"
if [ "$want" = "$got" ] && echo "$output" | grep -q " 0 error(s)"; then
    echo "Script checks test passed."
    exit 0
fi
echo "Script checks test FAILED:"
echo "$output"
exit 1
