#!/usr/bin/env bash
# Tests the editor's multiplayer for real: starts a dedicated server and two
# clients (separate programs, no windows) on this computer, and checks that
# they connect, call each other's RPCs, keep nodes in sync, spawn players
# and see each other leave. Uses editor/tests/multiplayer.
#
#   scripts/test-multiplayer.sh [debug|release]   build the editor, then test
#   SPROUT_EDITOR=path/to/sprout_editor scripts/test-multiplayer.sh   test that editor
set -u
source "$(dirname "$0")/_common.sh"

editor="${SPROUT_EDITOR:-}"
if [ -z "$editor" ]; then
    configure || exit 1
    cmake --build "$BUILD" --target sprout_editor --parallel "$JOBS" || exit 1
    editor="$EDITOR"
fi
project="$ROOT/editor/tests/multiplayer"
port="${SPROUT_TEST_PORT:-7790}"
logs="$(mktemp -d)"

# Build the test's scripts first, so the three programs don't all try to at once
"$editor" --project "$project" --check-scripts > "$logs/build.txt" 2>&1 || { cat "$logs/build.txt"; exit 1; }

"$editor" --project "$project" --play --server --frames 900 -- --port "$port" > "$logs/server.txt" 2>&1 &
server=$!
sleep 1
"$editor" --project "$project" --play --headless --frames 780 -- --port "$port" --expect-leave > "$logs/client1.txt" 2>&1 &
client1=$!
sleep 1
"$editor" --project "$project" --play --headless --frames 360 -- --port "$port" > "$logs/client2.txt" 2>&1 &
client2=$!
wait $server $client1 $client2

ok=1
for who in server client1 client2; do
    echo "--- $who"
    grep -E "CHECK|PASSED|FAILED|Couldn.t|rror|ball moved" "$logs/$who.txt"
done
grep -q "SERVER PASSED" "$logs/server.txt" || ok=0
grep -q "CLIENT PASSED" "$logs/client1.txt" || ok=0
grep -q "CLIENT PASSED" "$logs/client2.txt" || ok=0
if [ $ok = 1 ]; then
    echo "Multiplayer test passed."
    exit 0
fi
echo "Multiplayer test FAILED (full output in $logs)."
exit 1
