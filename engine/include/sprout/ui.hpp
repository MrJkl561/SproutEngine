#pragma once
// =============================================================================
//  sprout/ui.hpp - buttons, sliders and other menu things (made with raygui)
//
//  Call these from your scene's drawUI(). They draw AND tell you what the
//  player did, all in one line:
//
//    void drawUI() override {
//        if (button("Play", {300, 200, 200, 50})) changeScene<Level1>();
//        if (button("Quit", {300, 270, 200, 50})) quitGame();
//        slider("Volume", {300, 340, 200, 20}, volume, 0.0f, 1.0f);
//        checkbox("Fullscreen", {300, 380, 20, 20}, fullscreen);
//    }
//
//  The Rect is {x, y, width, height} of the control on the screen.
// =============================================================================
#include <string>

#include "sprout/math.hpp"

namespace sprout {

/// A clickable button. Returns true on the frame it is clicked.
bool button(const std::string& text, const Rect& area);
/// A checkbox. Flips `checked` when clicked; returns true if it changed.
bool checkbox(const std::string& text, const Rect& area, bool& checked);
/// A slider from min to max. Changes `value`; returns true if it changed.
bool slider(const std::string& text, const Rect& area, float& value, float min, float max);
/// A box the player can type in. Click it to start typing, press Enter to finish.
/// Returns true if the text changed.
bool textBox(const Rect& area, std::string& text, int maxLength = 32);
/// A bar that is `value / max` full - for health, loading, etc.
void progressBar(const Rect& area, float value, float max = 1.0f);
/// Plain text in the UI style.
void label(const std::string& text, const Rect& area);
/// A box with a title, to put other controls inside.
void panel(const std::string& title, const Rect& area);

/// How big the UI text is (20 by default). Stays the same when you change theme.
void setUITextSize(int size);

enum class UITheme { Default, Dark, Candy, Cyber, Jungle, Sunny, Terminal };
/// Changes how all the buttons, sliders, etc. look. Each theme comes with its
/// own pixel font and text size (unless you picked one with setUITextSize()).
/// Note: the Candy theme's font draws '>' like '<', so avoid those on its buttons.
void setUITheme(UITheme theme);

} // namespace sprout
