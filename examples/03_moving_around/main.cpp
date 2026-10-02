// =============================================================================
//  Example 03 - Moving Around
//
//  What you'll learn:
//    - moving a player with the keyboard (arrow keys or WASD)
//    - what `dt` (delta time) is, and why we multiply speeds by it
//    - keeping the player on the screen with clamp()
//    - the difference between isKeyDown() and isKeyPressed()
//    - reading the mouse: where it is and when it's clicked
//
//  Controls:
//    Arrow keys / WASD   move the yellow ball
//    Hold Shift          run faster              (uses isKeyDown)
//    Press Space         change the ball's color (uses isKeyPressed)
//    Mouse               the little dot follows the mouse
//    Left click          give the dot a new random color
// =============================================================================
#include <sprout/sprout.hpp>

using namespace sprout;

struct MovingScene : Scene {
    Vec2 playerPosition{400, 225}; // the middle of the ball
    float playerRadius = 25;
    float walkSpeed = 250;         // in pixels per SECOND
    float runSpeed = 500;
    Color playerColor = Color::Yellow;
    int spacePresses = 0;

    Color mouseDotColor = Color::White;

    void start() override {
        background = Color::fromHex(0x223344);
        hideCursor(); // we draw our own mouse dot instead
    }

    void update(float dt) override {
        // ---- What is dt? -------------------------------------------------------
        // The game runs about 60 times per second. Each time is called a "frame".
        // dt ("delta time") is how many seconds the LAST frame took - usually
        // about 0.016 (that's 1/60 of a second).
        //
        // If we moved 5 pixels every frame, a fast computer (144 frames per
        // second) would move the player much faster than a slow one (30 frames
        // per second). Not fair!
        //
        // Instead we say "move 250 pixels per SECOND" and multiply by dt:
        //     250 * 0.016 = 4 pixels this frame.
        // Now the player moves at the same speed on every computer.

        // ---- isKeyDown: true EVERY frame while the key is held ------------------
        float speed = walkSpeed;
        if (isKeyDown(Key::LeftShift) || isKeyDown(Key::RightShift)) {
            speed = runSpeed;
        }

        // getVector gives us the direction the player wants to go, e.g.
        // {1, 0} for right, {0, -1} for up, {0.7, 0.7} for down-right.
        Vec2 direction = getVector("left", "right", "up", "down");
        playerPosition += direction * speed * dt;

        // ---- Stay on the screen -------------------------------------------------
        // clamp(value, min, max) keeps a number between min and max.
        // We use the radius so the WHOLE ball stays visible, not just its middle.
        playerPosition.x = clamp(playerPosition.x, playerRadius, screenWidth() - playerRadius);
        playerPosition.y = clamp(playerPosition.y, playerRadius, screenHeight() - playerRadius);

        // ---- isKeyPressed: true only on the ONE frame the key goes down ---------
        // If we used isKeyDown here, the color would change 60 times a second
        // while you hold Space - way too fast! isKeyPressed changes it once per press.
        if (isKeyPressed(Key::Space)) {
            playerColor = randomColor();
            spacePresses = spacePresses + 1;
        }

        // ---- The mouse ------------------------------------------------------------
        if (isMousePressed(MouseButton::Left)) {
            mouseDotColor = randomColor();
            // A little burst of particles where you clicked. Just for fun!
            particles.burst(mousePosition(), mouseDotColor, 15, 120);
        }
    }

    void draw() override {
        Vec2 mouse = mousePosition();

        // A line from the player to the mouse, so you can see them both.
        drawLine(playerPosition, mouse, Color::White.withAlpha(0.15f), 2);

        // The player: a ball with a darker border.
        drawCircle(playerPosition, playerRadius, playerColor);
        drawCircleOutline(playerPosition, playerRadius, playerColor.darker(0.4f), 4);

        // The mouse dot.
        drawCircle(mouse, 8, mouseDotColor);
        drawCircleOutline(mouse, 14, mouseDotColor.withAlpha(0.5f), 2);
    }

    void drawUI() override {
        drawTextCentered("Arrows / WASD: move     Shift: run     Space: new color     Click: color the dot",
                         Vec2{400, 20}, 15, Color::LightGray);

        // Show what the program "sees" right now. Try holding and tapping keys!
        bool running = isKeyDown(Key::LeftShift) || isKeyDown(Key::RightShift);
        drawText(std::string("Shift held: ") + (running ? "YES" : "no"), 10, 375, 20,
                 running ? Color::Lime : Color::Gray);
        drawText("Space pressed " + std::to_string(spacePresses) + " times", 10, 400, 20, Color::Gray);

        // Vec2 holds decimal numbers; static_cast<int> turns them into whole numbers for showing.
        int mouseX = static_cast<int>(mousePosition().x);
        int mouseY = static_cast<int>(mousePosition().y);
        drawText("Mouse: " + std::to_string(mouseX) + ", " + std::to_string(mouseY), 10, 425, 20, Color::Gray);
    }
};

int main() {
    Game game("03 - Moving Around", 800, 450);
    game.run<MovingScene>();
}
