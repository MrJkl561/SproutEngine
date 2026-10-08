#!/usr/bin/env bash
# Builds the engine's unit tests and runs them.
#
#   scripts/test.sh            Release
#   scripts/test.sh debug      Debug
set -e
source "$(dirname "$0")/_common.sh"
configure -DSPROUT_BUILD_TESTS=ON
cmake --build "$BUILD" --parallel "$JOBS"
ctest --test-dir "$BUILD" --output-on-failure
echo
echo "All tests passed."
