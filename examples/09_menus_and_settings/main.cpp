// =============================================================================
//  Example 09 - Menus and Settings
//
//  Almost every game has a title menu and a settings screen. Here's how!
//
//  What you'll learn:
//    - several scenes that send you back and forth (title, settings, play)
//    - buttons, a slider, checkboxes and a text box from ui.hpp
//    - changing the look of all the buttons with setUITheme()
//    - remembering settings with SaveFile, so they're still there next time
//
//  Controls:
//    Mouse    click the buttons, drag the slider, click the text box to type
//    Enter    stop typing in the text box
//    Escape   go back (or quit, on the title screen)
// =============================================================================
#include <sprout/sprout.hpp>

#include <cmath> // std::abs
#include <string>
#include <vector>

using namespace sprout;

// All the scenes use the same save file for the settings.
const std::string SettingsFile = "settings.json";

// The UI themes the player can pick from, and their names in the same order.
const std::vector<UITheme> Themes = {UITheme::Default, UITheme::Dark,  UITheme::Candy,   UITheme::Cyber,
                                     UITheme::Jungle,  UITheme::Sunny, UITheme::Terminal};
const std::vector<std::string> ThemeNames = {"Default", "Dark", "Candy", "Cyber", "Jungle", "Sunny", "Terminal"};

// Reads the save file and switches everything on, the way the player left it.
// main() calls this once, when the game starts.
void applySavedSettings() {
    SaveFile settings(SettingsFile);
    // get(name, default): the default is used the very first time, when nothing is saved yet.
    setMasterVolume(settings.get("volume", 0.8f));
    setFullscreen(settings.get("fullscreen", false));
    int themeNumber = settings.get("theme", 0);
    if (themeNumber >= 0 && themeNumber < static_cast<int>(Themes.size())) {
        setUITheme(Themes[themeNumber]);
    }
}

// ---------------------------------------------------------------------------
//  The title screen. Its drawUI() is written at the very bottom of the file,
//  because it needs to know about the other two scenes.
// ---------------------------------------------------------------------------
struct TitleScreen : Scene {
    std::string playerName;
    bool showFps = false;

    void start() override {
        background = Color::fromHex(0x1F1D36);
        SaveFile settings(SettingsFile);
        playerName = settings.get("name", "Player");
        showFps = settings.get("showFps", false);
    }

    void update(float) override {
        if (isKeyPressed(Key::Escape)) quitGame();
    }

    void draw() override {
        // Some soft bubbles floating up, so the menu doesn't look empty.
        // wrap() makes each bubble start again at the bottom when it leaves the top.
        for (int i = 0; i < 12; i++) {
            float x = 40 + i * 66;
            float speed = 20 + (i % 4) * 10;  // some bubbles are faster than others
            float y = wrap(500 - timeSinceStart() * speed - i * 173, -50, 500);
            drawCircle(Vec2{x, y}, 12 + (i % 3) * 8, Color::Purple.withAlpha(0.15f));
        }
    }

    void drawUI() override; // written at the bottom
};

// ---------------------------------------------------------------------------
//  The settings screen
// ---------------------------------------------------------------------------
struct SettingsScreen : Scene {
    SaveFile settings{SettingsFile};
    std::string playerName;
    float volume = 0.8f;
    bool showFps = false;
    bool fullscreen = false;
    int themeNumber = 0;

    void start() override {
        background = Color::fromHex(0x1F1D36);
        // Start with whatever was saved last time.
        playerName = settings.get("name", "Player");
        volume = settings.get("volume", 0.8f);
        showFps = settings.get("showFps", false);
        fullscreen = settings.get("fullscreen", false);
        themeNumber = settings.get("theme", 0);
    }

    void update(float) override {
        if (isKeyPressed(Key::Escape)) changeScene<TitleScreen>();
    }

    // Moves to the next (+1) or previous (-1) theme in the list, going round in a circle.
    void changeTheme(int step) {
        themeNumber = themeNumber + step;
        if (themeNumber < 0) themeNumber = static_cast<int>(Themes.size()) - 1;
        if (themeNumber >= static_cast<int>(Themes.size())) themeNumber = 0;
        setUITheme(Themes[themeNumber]);
        settings.set("theme", themeNumber);
    }

    void drawUI() override {
        // A panel is a box with a title. The other controls go on top of it.
        panel("Settings", Rect{150, 40, 500, 370});

        // Each control returns true when the player changed it. That's the moment
        // to use the new value AND save it, so it's remembered next time.
        label("Your name", Rect{180, 95, 130, 30});
        if (textBox(Rect{320, 95, 280, 30}, playerName, 16)) {
            settings.set("name", playerName);
        }

        // (A slider can show its own label, but we use label() so everything lines up.)
        label("Volume", Rect{180, 145, 130, 30});
        if (slider("", Rect{320, 150, 220, 20}, volume, 0, 1)) {
            setMasterVolume(volume);
            settings.set("volume", volume);
        }

        label("Options", Rect{180, 190, 130, 30});
        if (checkbox("Show FPS", Rect{320, 193, 24, 24}, showFps)) {
            settings.set("showFps", showFps);
        }
        if (checkbox("Fullscreen", Rect{320, 228, 24, 24}, fullscreen)) {
            setFullscreen(fullscreen);
            settings.set("fullscreen", fullscreen);
        }

        // A theme picker made from two buttons with the theme's name between them.
        label("Theme", Rect{180, 275, 130, 30});
        if (button("Prev", Rect{320, 275, 70, 30})) changeTheme(-1);
        label(ThemeNames[themeNumber], Rect{405, 275, 110, 30});
        if (button("Next", Rect{530, 275, 70, 30})) changeTheme(1);

        if (button("Back", Rect{300, 340, 200, 45})) {
            changeScene<TitleScreen>();
        }

        if (showFps) drawFPS(700, 10);
    }
};

// ---------------------------------------------------------------------------
//  The "game". Tiny, but it greets you by name!
// ---------------------------------------------------------------------------
struct PlayScene : Scene {
    std::string playerName;
    bool showFps = false;
    Vec2 ballPosition{400, 300};
    Vec2 ballVelocity{220, -180};

    void start() override {
        background = Color::fromHex(0x163A4A);
        SaveFile settings(SettingsFile);
        playerName = settings.get("name", "Player");
        showFps = settings.get("showFps", false);
    }

    void update(float dt) override {
        if (isKeyPressed(Key::Escape)) changeScene<TitleScreen>();

        // A ball bouncing around the bottom part of the screen.
        ballPosition += ballVelocity * dt;
        // std::abs makes a number positive, so the ball always bounces AWAY from the edge.
        if (ballPosition.x < 20) ballVelocity.x = std::abs(ballVelocity.x);
        if (ballPosition.x > 780) ballVelocity.x = -std::abs(ballVelocity.x);
        if (ballPosition.y < 220) ballVelocity.y = std::abs(ballVelocity.y);
        if (ballPosition.y > 430) ballVelocity.y = -std::abs(ballVelocity.y);
    }

    void draw() override {
        drawCircle(ballPosition, 20, Color::fromHSV(timeSinceStart() * 60));
    }

    void drawUI() override {
        drawTextCentered("Hello, " + playerName + "!", Vec2{400, 110}, 50, Color::Gold);
        drawTextCentered("This is where your game would go.", Vec2{400, 165}, 20, Color::LightGray);

        if (button("Back", Rect{20, 20, 120, 40})) {
            changeScene<TitleScreen>();
        }
        if (showFps) drawFPS(700, 10);
    }
};

// Here's the title screen's drawUI(). It's down here because its buttons
// open SettingsScreen and PlayScene, which are written above.
void TitleScreen::drawUI() {
    drawTextCentered("SPROUT QUEST", Vec2{400, 85}, 60, Color::Gold);
    drawTextCentered("Welcome, " + playerName + "!", Vec2{400, 140}, 20, Color::LightGray);

    // button() draws a button AND tells us if it was clicked this frame.
    if (button("Play", Rect{300, 190, 200, 50})) changeScene<PlayScene>();
    if (button("Settings", Rect{300, 255, 200, 50})) changeScene<SettingsScreen>();
    if (button("Quit", Rect{300, 320, 200, 50})) quitGame();

    if (showFps) drawFPS(700, 10);
}

int main() {
    Game game("09 - Menus and Settings", 800, 450);
    setExitKey(Key::None); // Escape shouldn't close the game - we use it for "go back"
    applySavedSettings();
    game.run<TitleScreen>();
}
