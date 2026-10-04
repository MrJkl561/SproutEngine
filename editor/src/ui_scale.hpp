// How big the editor's interface is drawn. The editor lays itself out in
// "UI pixels" and draws them uiScale screen pixels big, so it looks the same
// size on a 4K laptop at 150% as on a normal monitor at 100%.
#pragma once

namespace s2d {

// The scale the operating system asks for (Windows display scaling, Xft.dpi on
// Linux), rounded to a quarter. SPROUT_EDITOR_UI_SCALE=1.5 overrides it.
// macOS already scales windows itself, so it's 1 there.
float systemUiScale();

}  // namespace s2d
