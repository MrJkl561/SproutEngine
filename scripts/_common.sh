# Shared by the other scripts (sourced, not run): reads [debug|release] and sets
#   ROOT    the repository folder
#   CONFIG  Release or Debug
#   BUILD   the build folder (build-release or build-debug)
#   EDITOR  the editor program inside it
#   GEN     the CMake generator options (Ninja when it's installed)
#   REST    any other arguments (an array)
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CONFIG=Release
REST=()
for arg in "$@"; do
    case "$(printf '%s' "$arg" | tr '[:upper:]' '[:lower:]')" in
        debug) CONFIG=Debug ;;
        release) CONFIG=Release ;;
        *) REST+=("$arg") ;;
    esac
done
BUILD="$ROOT/build-$(printf '%s' "$CONFIG" | tr '[:upper:]' '[:lower:]')"
EDITOR="$BUILD/bin/sprout_editor/sprout_editor"
GEN=()
if command -v ninja >/dev/null 2>&1; then GEN=(-G Ninja); fi

if ! command -v cmake >/dev/null 2>&1; then
    echo "CMake was not found. Install it first:"
    echo "  Ubuntu/Debian: sudo apt install cmake ninja-build g++ libx11-dev libxrandr-dev libxinerama-dev \\"
    echo "                 libxcursor-dev libxi-dev libgl1-mesa-dev libasound2-dev"
    echo "  macOS:         brew install cmake ninja   (and Xcode's command line tools)"
    exit 1
fi

# Number of CPU cores, for building in parallel
JOBS="$(getconf _NPROCESSORS_ONLN 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)"

configure() {
    cmake -S "$ROOT" -B "$BUILD" "${GEN[@]}" -DCMAKE_BUILD_TYPE="$CONFIG" "$@"
}
