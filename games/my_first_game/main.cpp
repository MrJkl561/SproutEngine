// =============================================================================
//  My First Game
//
//  This is YOUR game - change anything you like!
//  Build it, then run:   build/bin/my_first_game/my_first_game
//  (on Windows:          build\bin\my_first_game\my_first_game.exe)
//
//  Want another game? Copy this whole folder, give the copy a new name, and
//  build again. Sprout finds it automatically.
// =============================================================================
#include <sprout/sprout.hpp>

using namespace sprout;

struct MyGame : Scene {
    Vec2 player{400, 225}; // where the player is
    float speed = 300;     // pixels per second

    void start() override {
        background = Color::DarkBlue;
    }

    void update(float dt) override {
        // Arrow keys or WASD move the player
        player += getVector("left", "right", "up", "down") * speed * dt;
    }

    void draw() override {
        drawCircle(player, 30, Color::Yellow);
    }

    void drawUI() override {
        drawText("Move with the arrow keys or WASD", 10, 10, 20);
    }
};

int main() {
    Game game("My First Game", 800, 450);
    game.run<MyGame>();
}
