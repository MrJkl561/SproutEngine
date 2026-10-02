// =============================================================================
//  Example 08 - Snake
//
//  Eat the apples, grow longer, and don't bump into the walls or yourself!
//
//  What you'll learn:
//    - games on a grid: thinking in columns and rows instead of pixels
//    - a std::vector as a list that grows (the snake's body)
//    - moving in steps with a Timer, instead of smoothly every frame
//    - randomInt() to put the food in a random free spot
//    - game over, score, and pausing with the `paused` switch
//
//  Controls:
//    Arrow keys / WASD   steer
//    P                   pause
//    R                   play again (after game over)
// =============================================================================
#include <sprout/sprout.hpp>

#include <cmath> // std::sin
#include <vector>

using namespace sprout;

const int CellSize = 25; // each grid square is 25 x 25 pixels
const int Columns = 32;  // 32 * 25 = 800 pixels wide
const int Rows = 18;     // 18 * 25 = 450 pixels high

// One square on the grid. x is the column, y is the row.
struct Cell {
    int x = 0;
    int y = 0;
};

// Are two cells the same square?
bool sameCell(Cell a, Cell b) {
    return a.x == b.x && a.y == b.y;
}

// The middle of a cell, in pixels. We need this to draw things.
Vec2 cellCenter(Cell cell) {
    return Vec2{cell.x * CellSize + CellSize / 2.0f, cell.y * CellSize + CellSize / 2.0f};
}

struct SnakeGame : Scene {
    // The snake is a list of cells. body[0] is the head, the last one is the tail.
    std::vector<Cell> body;
    Cell direction{1, 0};     // which way we're going: {1, 0} = right
    Cell nextDirection{1, 0}; // which way we'll turn at the next step
    Cell food;
    int score = 0;
    bool gameOver = false;
    Timer stepTimer{0.13f};   // the snake moves one square every 0.13 seconds

    void start() override {
        background = Color::fromHex(0x20242C);
        // A snake 3 squares long, in the middle-left, heading right.
        body.push_back(Cell{8, 9}); // head
        body.push_back(Cell{7, 9});
        body.push_back(Cell{6, 9}); // tail
        placeFood();
    }

    void placeFood() {
        // Pick random squares until we find one the snake isn't on.
        bool onSnake = true;
        while (onSnake) {
            food = Cell{randomInt(0, Columns - 1), randomInt(0, Rows - 1)};
            onSnake = false;
            for (Cell part : body) {
                if (sameCell(part, food)) onSnake = true;
            }
        }
    }

    void update(float) override {
        if (gameOver) {
            if (isKeyPressed(Key::R)) restartScene();
            return;
        }

        // Pausing: the Timer stops counting while the scene is paused.
        if (isActionPressed("pause")) {
            paused = !paused; // ! means "not", so this flips true <-> false
        }
        if (paused) return;

        // Remember which way the player wants to go. You can't turn straight
        // back into yourself (from right to left, say), so we check that we're
        // not reversing. We compare with `direction` (where we're REALLY going).
        if (isActionPressed("up") && direction.y != 1) nextDirection = Cell{0, -1};
        if (isActionPressed("down") && direction.y != -1) nextDirection = Cell{0, 1};
        if (isActionPressed("left") && direction.x != 1) nextDirection = Cell{-1, 0};
        if (isActionPressed("right") && direction.x != -1) nextDirection = Cell{1, 0};

        // Time for the next step?
        if (stepTimer.isDone()) {
            stepTimer.restart();
            moveSnake();
        }
    }

    void moveSnake() {
        direction = nextDirection;
        Cell newHead = Cell{body[0].x + direction.x, body[0].y + direction.y};

        // Hit a wall?
        bool hitWall = newHead.x < 0 || newHead.x >= Columns || newHead.y < 0 || newHead.y >= Rows;
        // Hit ourselves? (The tail is about to move away, so it doesn't count.)
        bool hitSelf = false;
        for (int i = 0; i < static_cast<int>(body.size()) - 1; i++) {
            if (sameCell(body[i], newHead)) hitSelf = true;
        }
        if (hitWall || hitSelf) {
            gameOver = true;
            camera.shake(10, 0.4f);
            particles.burst(cellCenter(body[0]), Color::Red, 40, 250, 0.8f, 5);
            return;
        }

        // Move: add a new head at the front...
        body.insert(body.begin(), newHead);

        if (sameCell(newHead, food)) {
            // ...and if we ate the food, keep the tail. That's how the snake grows!
            score = score + 1;
            particles.burst(cellCenter(food), Color::Red, 20, 150);
            placeFood();
            // Get a little faster each time, but not TOO fast.
            float newStepTime = stepTimer.duration() * 0.97f;
            if (newStepTime < 0.06f) newStepTime = 0.06f;
            stepTimer.restart(newStepTime);
        } else {
            // ...otherwise remove the last piece of the tail, so the length stays the same.
            body.pop_back();
        }
    }

    void draw() override {
        // A checkerboard so it's easy to see the grid.
        for (int x = 0; x < Columns; x++) {
            for (int y = 0; y < Rows; y++) {
                if ((x + y) % 2 == 0) {
                    drawRect(x * CellSize, y * CellSize, CellSize, CellSize, Color::fromHex(0x272C36));
                }
            }
        }

        // The food: an apple that gently pulses.
        float pulse = 1 + std::sin(timeSinceStart() * 6) * 0.1f;
        Vec2 apple = cellCenter(food);
        drawCircle(apple, 10 * pulse, Color::Red);
        drawRect(apple.x - 1, apple.y - 14, 3, 6, Color::Brown);  // the stem
        drawCircle(apple + Vec2{5, -11}, 3, Color::Green);         // a little leaf

        // The snake, from the tail to the head, getting brighter towards the head.
        for (int i = static_cast<int>(body.size()) - 1; i >= 0; i--) {
            float howFarAlong = static_cast<float>(i) / body.size(); // 0 at the head, almost 1 at the tail
            Color color = Color::mix(Color::fromHex(0xA8E05F), Color::fromHex(0x2E8B57), howFarAlong);
            Rect square{body[i].x * CellSize + 1, body[i].y * CellSize + 1, CellSize - 2, CellSize - 2};
            drawRoundedRect(square, 6, color);
        }

        // Eyes on the head, looking the way we're going.
        Vec2 head = cellCenter(body[0]);
        Vec2 forward{direction.x * 4.0f, direction.y * 4.0f};
        Vec2 side{direction.y * 5.0f, direction.x * 5.0f}; // sideways from "forward"
        drawCircle(head + forward + side, 3.5f, Color::White);
        drawCircle(head + forward - side, 3.5f, Color::White);
        drawCircle(head + forward * 1.4f + side, 1.5f, Color::Black);
        drawCircle(head + forward * 1.4f - side, 1.5f, Color::Black);
    }

    void drawUI() override {
        drawText("Score: " + std::to_string(score), 10, 8, 30, Color::White.withAlpha(0.8f));

        if (paused) {
            drawTextCentered("PAUSED - press P", screenCenter(), 40, Color::White);
        }
        if (gameOver) {
            drawRect(0, 0, screenWidth(), screenHeight(), Color::Black.withAlpha(0.6f));
            drawTextCentered("GAME OVER", Vec2{400, 170}, 60, Color::Red);
            drawTextCentered("You ate " + std::to_string(score) + " apples", Vec2{400, 235}, 30, Color::White);
            drawTextCentered("Press R to play again", Vec2{400, 290}, 20, Color::LightGray);
        }
    }
};

int main() {
    Game game("08 - Snake", 800, 450);
    game.run<SnakeGame>();
}
