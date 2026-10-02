// =============================================================================
//  Example 01 - Hello World
//
//  The smallest possible Sprout game: open a window and write some text.
// =============================================================================
#include <sprout/sprout.hpp>

using namespace sprout;

// A scene is one "screen" of your game.
struct HelloScene : Scene {
    // draw() runs every frame. Draw everything you want to see here.
    void draw() override {
        drawTextCentered("Hello, World!", screenCenter(), 60, Color::Gold);
        drawTextCentered("Press Escape to quit", screenCenter() + Vec2{0, 70}, 20, Color::LightGray);
    }
};

int main() {
    Game game("Hello World", 800, 450); // opens an 800 x 450 window
    game.run<HelloScene>();             // runs HelloScene until the window closes
}
