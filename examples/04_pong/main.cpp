// =============================================================================
//  Example 04 - Pong
//
//  The classic two-player game! Bounce the ball past your friend's paddle.
//
//  What you'll learn:
//    - making your own input "actions" with bindAction()
//    - a ball that moves with a velocity and bounces off walls
//    - checking if two rectangles touch with Rect::overlaps()
//    - keeping score and resetting the ball after a point
//    - waiting a moment with after()
//    - winning, and restarting with restartScene()
//
//  Controls:
//    Left player:   W / S
//    Right player:  Up / Down arrows
//    R              play again (after someone wins)
// =============================================================================
#include <sprout/sprout.hpp>

#include <cmath> // std::abs

using namespace sprout;

const int PointsToWin = 5;

struct PongScene : Scene {
    // Paddles are just rectangles: {x, y, width, height}. x and y are the top-left.
    Rect leftPaddle{30, 175, 16, 100};
    Rect rightPaddle{754, 175, 16, 100};
    float paddleSpeed = 420;

    // The ball has a position (its middle) and a velocity (how fast it moves
    // in x and y, in pixels per second).
    Vec2 ballPosition{400, 225};
    Vec2 ballVelocity{0, 0};
    float ballSize = 16;

    int leftScore = 0;
    int rightScore = 0;
    std::string winner = ""; // empty while nobody has won yet
    int serveDirection = 1;  // +1 = serve to the right, -1 = to the left

    void start() override {
        background = Color::fromHex(0x111522);
        // Serve towards a random player after a short pause.
        serveBall(chance(0.5f) ? 1 : -1);
    }

    // Puts the ball in the middle and, after a second, sends it towards
    // one player. direction is +1 (to the right) or -1 (to the left).
    void serveBall(int direction) {
        ballPosition = screenCenter();
        ballVelocity = {0, 0};
        serveDirection = direction;
        // after(seconds, code) runs the code later. [this] lets the code use
        // this scene's functions and variables.
        after(1.0f, [this] { launchBall(); });
    }

    void launchBall() {
        // 300 pixels per second sideways, and a random bit up or down.
        ballVelocity = Vec2{300.0f * serveDirection, randomFloat(-150, 150)};
    }

    // The ball's hitbox, made from its middle and its size.
    Rect ballRect() {
        return Rect::fromCenter(ballPosition, Vec2{ballSize, ballSize});
    }

    void update(float dt) override {
        if (winner != "") {
            if (isKeyPressed(Key::R)) {
                restartScene(); // starts this scene again from scratch: scores back to 0
            }
            return; // the game is over, so don't move anything
        }

        // ---- Move the paddles ---------------------------------------------------
        // getAxis gives -1 (up), +1 (down) or 0 (no key / both keys).
        leftPaddle.y += getAxis("left_paddle_up", "left_paddle_down") * paddleSpeed * dt;
        rightPaddle.y += getAxis("right_paddle_up", "right_paddle_down") * paddleSpeed * dt;
        // Keep them on the screen.
        leftPaddle.y = clamp(leftPaddle.y, 0, screenHeight() - leftPaddle.height);
        rightPaddle.y = clamp(rightPaddle.y, 0, screenHeight() - rightPaddle.height);

        // ---- Move the ball ------------------------------------------------------
        ballPosition += ballVelocity * dt;

        // Bounce off the top and bottom walls by flipping the y speed.
        if (ballPosition.y < ballSize / 2) {
            ballPosition.y = ballSize / 2;
            ballVelocity.y = std::abs(ballVelocity.y); // now moving down
        }
        if (ballPosition.y > screenHeight() - ballSize / 2) {
            ballPosition.y = screenHeight() - ballSize / 2;
            ballVelocity.y = -std::abs(ballVelocity.y); // now moving up
        }

        // ---- Bounce off the paddles -------------------------------------------
        // We only bounce if the ball is moving TOWARDS the paddle. Otherwise it
        // could get stuck inside the paddle, bouncing back and forth.
        if (ballVelocity.x < 0 && ballRect().overlaps(leftPaddle)) {
            bounceOffPaddle(leftPaddle, 1);
        }
        if (ballVelocity.x > 0 && ballRect().overlaps(rightPaddle)) {
            bounceOffPaddle(rightPaddle, -1);
        }

        // ---- Did someone score? -------------------------------------------------
        if (ballPosition.x < -ballSize) { // went past the left edge
            rightScore = rightScore + 1;
            pointScored(-1);
        }
        if (ballPosition.x > screenWidth() + ballSize) { // went past the right edge
            leftScore = leftScore + 1;
            pointScored(1);
        }
    }

    void bounceOffPaddle(const Rect& paddle, int newDirection) {
        // Where did the ball hit the paddle? -1 = top edge, 0 = middle, +1 = bottom edge.
        // Hitting near the edge sends the ball off at a steeper angle. This gives
        // players a way to aim!
        float hitSpot = (ballPosition.y - paddle.center().y) / (paddle.height / 2);
        hitSpot = clamp(hitSpot, -1, 1);

        // Speed up a little every hit, so long rallies get exciting.
        float speed = std::abs(ballVelocity.x) * 1.07f;
        if (speed > 900) speed = 900;

        ballVelocity = Vec2{speed * newDirection, hitSpot * 350};

        // Some juice: sparks and a tiny screen shake.
        particles.burst(ballPosition, Color::Cyan, 12, 200, 0.3f);
        camera.shake(3, 0.1f);
    }

    // direction: which way the next serve goes (towards the player who lost the point)
    void pointScored(int direction) {
        camera.shake(8, 0.3f);
        if (leftScore >= PointsToWin) {
            winner = "Left player";
        } else if (rightScore >= PointsToWin) {
            winner = "Right player";
        }

        if (winner == "") {
            serveBall(direction);
        } else {
            ballPosition = screenCenter();
            ballVelocity = {0, 0};
            particles.burst(screenCenter(), Color::Gold, 80, 400, 1.2f, 6);
        }
    }

    void draw() override {
        // A dashed line down the middle: lots of short rectangles.
        for (float y = 5; y < screenHeight(); y += 30) {
            drawRect(397, y, 6, 18, Color::DarkGray);
        }

        drawRoundedRect(leftPaddle, 6, Color::SkyBlue);
        drawRoundedRect(rightPaddle, 6, Color::Pink);

        if (winner == "") {
            drawRect(ballRect(), Color::White);
        }
    }

    void drawUI() override {
        drawTextCentered(std::to_string(leftScore), Vec2{300, 50}, 60, Color::SkyBlue);
        drawTextCentered(std::to_string(rightScore), Vec2{500, 50}, 60, Color::Pink);
        drawTextCentered("W / S", Vec2{100, 430}, 20, Color::DarkGray);
        drawTextCentered("Up / Down", Vec2{700, 430}, 20, Color::DarkGray);

        if (winner != "") {
            drawTextCentered(winner + " wins!", Vec2{400, 200}, 50, Color::Gold);
            drawTextCentered("Press R to play again", Vec2{400, 260}, 20, Color::White);
        }
    }
};

int main() {
    Game game("04 - Pong", 800, 450);

    // Our own actions! An action is a name for something a player does.
    // We set them up once, before the game starts. Later we just ask
    // isActionDown("left_paddle_up") and don't care which key it is -
    // so changing the controls only means changing these lines.
    bindAction("left_paddle_up", Key::W);
    bindAction("left_paddle_down", Key::S);
    bindAction("right_paddle_up", Key::Up);
    bindAction("right_paddle_down", Key::Down);

    game.run<PongScene>();
}
