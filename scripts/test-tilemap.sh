#!/usr/bin/env bash
# Tests tile maps and the UI nodes:
#   - imports the Tiled maps in editor/tests/tilemap/maps (CSV, base64 + zlib,
#     base64 + gzip, the old XML style, JSON, an infinite map, external tile
#     sets, objects, image layers, groups) into a copy of the test project and
#     checks the nodes they become; a zstd map must fail with a clear message
#   - plays the project's main scene (in a window; on Linux without a display,
#     through xvfb-run): bodies landing on solid tiles and falling through
#     others, the TileMap functions scripts use, clicks on buttons in a
#     CanvasLayer and in the world, a VBoxContainer's layout, and the colours
#     drawn (flipped tiles, a ProgressBar)
#
#   scripts/test-tilemap.sh [debug|release]   build the editor, then test
#   SPROUT_EDITOR=path/to/sprout_editor scripts/test-tilemap.sh   test that editor
set -u
source "$(dirname "$0")/_common.sh"

editor="${SPROUT_EDITOR:-}"
if [ -z "$editor" ]; then
    configure || exit 1
    cmake --build "$BUILD" --target sprout_editor --parallel "$JOBS" || exit 1
    editor="$EDITOR"
fi
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
cp -r "$ROOT/editor/tests/tilemap/." "$work/"
project="$work/project"
run=()
if [ "$(uname)" = "Linux" ] && [ -z "${DISPLAY:-}" ] && command -v xvfb-run >/dev/null; then run=(xvfb-run -a); fi

failed=0
expect() {  # expect "what" "output" "text it must contain"
    if printf '%s' "$2" | grep -qF -- "$3"; then
        echo "  ok: $1"
    else
        echo "  FAILED: $1 (wanted: $3)"
        failed=1
    fi
}

echo "Importing the Tiled maps..."
for map in level_csv.tmx level_zlib.tmx level_xml.tmx level_gzip.tmj level_infinite.tmj; do
    out="$("$editor" --project "$project" --import-tiled "$work/maps/$map" 2>&1)"
    expect "$map imports" "$out" "Imported $map"
done
out="$("$editor" --project "$project" --import-tiled "$work/maps/broken.tmx" 2>&1)"
expect "a zstd map says what to do" "$out" "compressed with zstd can't be read"
[ -f "$project/assets/tiles/tiles.png" ] && echo "  ok: the tile set's picture was copied in" || { echo "  FAILED: assets/tiles/tiles.png wasn't copied"; failed=1; }

ground="TileMap at=0,0 cells=12 src=assets/tiles/tiles.png solid=0;2; type1=spikes alpha=255 list=0,0:0/3/1 0,3:0/0/0 1,3:0/0/0 2,3:0/0/0 3,3:0/0/0 4,2:0/1/0 4,3:0/0/0 5,3:0/0/0 6,3:0/0/0 7,3:0/0/0 8,3:0/0/0 9,3:0/2/0"
echo "Checking the imported scenes..."
for scene in level_csv level_zlib level_xml level_gzip; do
    out="$("${run[@]}" "$editor" --project "$project" --play --scene "scenes/$scene.scene.json" --frames 2 2>&1)"
    expect "$scene: the tiles, flips, solid tiles and types" "$out" "NODE $scene/Ground $ground"
done
out="$("${run[@]}" "$editor" --project "$project" --play --scene scenes/level_csv.scene.json --frames 2 2>&1)"
expect "a layer's offset and opacity" "$out" "NODE level_csv/Decor TileMap at=8,4 cells=1"
expect "...its opacity" "$out" "alpha=127"
expect "a rectangle object -> Area2D" "$out" "NODE level_csv/Objects/Goal/CollisionShape2D CollisionShape2D at=8,16 size=16x32"
expect "a 'solid' object -> StaticBody2D" "$out" "NODE level_csv/Objects/solid StaticBody2D at=0,0"
expect "a point object" "$out" "NODE level_csv/Objects/Spawn Node2D at=16,40"
expect "a text object -> Label" "$out" "NODE level_csv/Objects/Sign Label at=20,4 text=Hello & welcome"
expect "a tile object -> Sprite2D" "$out" "NODE level_csv/Objects/Coin Sprite2D at=64,48 tex=assets/tiles/tiles.png frame=1"
expect "an image layer" "$out" "NODE level_csv/Sky Sprite2D at=2,3 tex=assets/tiles/sky.png"
expect "a group's layers" "$out" "NODE level_csv/Group/Inner TileMap at=0,0 cells=1"
out="$("${run[@]}" "$editor" --project "$project" --play --scene scenes/level_infinite.scene.json --frames 2 2>&1)"
expect "an infinite map's chunks" "$out" "cells=3 src=assets/tiles/tiles.png solid=0;2; type1=spikes alpha=255 list=-1,15:0/3/2 0,0:0/0/0 1,1:0/1/0"

echo "Playing the main scene (collisions, the UI, colours)..."
# (game pixels: red ground | a flipped and an unflipped half-white tile | the ProgressBar's fill and background)
export SPROUT_TEST_CLICKS="250,20@5;210,90@10;110,70@15;5,5@20"
export SPROUT_TEST_PIXELS="60,136=255,0,0;58,40=0,0,0;70,40=255,255,255;74,40=255,255,255;86,40=0,0,0;220,155=0,255,0;290,155=0,0,0"
out="$("${run[@]}" "$editor" --project "$project" --play --frames 70 2>&1)"
unset SPROUT_TEST_CLICKS SPROUT_TEST_PIXELS
expect "the TileMap functions" "$out" "MAPAPI cell=5,8 tile=0 now=2 solid=100 type=grass empty=-1"
expect "clicks, signals and layout" "$out" "UI play=1 world=1 toggles=1 sound=on quitY=34 quitW=110 menuH=96"
expect "a body lands on solid tiles" "$out" "BODY Faller floor=yes y=122"
expect "a body falls through a tile that isn't solid" "$out" "BODY Ghost floor=no"
expect "the colours drawn" "$out" "PIXELS OK"

out="$("${run[@]}" "$editor" --project "$project" --play --frames 300 -- --quit-test 2>&1)"
expect "quit() ends the game" "$out" "QUITTING"
if printf '%s' "$out" | grep -q "BODY Faller"; then echo "  FAILED: the game kept running after quit()"; failed=1; fi
if printf '%s' "$out" | grep -q "WRONG"; then echo "  FAILED: the TileMap's Vector2 functions"; failed=1; fi

if [ "$failed" = 0 ]; then
    echo "TileMap and UI test passed."
    exit 0
fi
echo "TileMap and UI test FAILED. The last game's output:"
echo "$out"
exit 1
