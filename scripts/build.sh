#!/usr/bin/env bash
# Builds everything: the engine, the examples, the games and the editor.
#
#   scripts/build.sh            Release
#   scripts/build.sh debug      a Debug build (slower, easier to debug)
#
# The programs end up in build-release/bin/ (or build-debug/bin/).
# Works on Linux (x86_64 and arm64) and macOS (Apple Silicon and Intel).
set -e
source "$(dirname "$0")/_common.sh"
echo "== Building Sprout ($CONFIG, $(uname -m)) in $BUILD =="
configure
cmake --build "$BUILD" --parallel "$JOBS"
echo
echo "Done. Programs are in $BUILD/bin/"
