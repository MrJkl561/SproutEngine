// =============================================================================
//  Example 07 - Platformer
//
//  Run, jump, grab the coins and reach the flag! There are two levels.
//
//  What you'll learn:
//    - building a level out of letters with TileMap
//    - placing the player, coins and the flag using markers in the map
//    - addSolid(map) so the player can stand on the ground
//    - gravity, jumping, and moveAndSlide() for walking into walls nicely
//    - isOnFloor(), so you can only jump when standing on something
//    - a camera that follows the player through a level wider than the screen
//    - giving a scene a value (the level number) through its constructor
//
//  Controls:
//    Left / Right (or A / D)   run
//    Space, Up or W            jump (let go early for a small hop)
//    Enter                     next level (on the "level complete" screen)
// =============================================================================
#include <sprout/sprout.hpp>

#include <cmath> // std::sin
#include <string>
#include <vector>

using namespace sprout;

// Each letter is one 32x32 tile:
//   # = ground     B = brick platform     C = coin
//   P = player start      E = exit flag   . = empty sky
const std::vector<std::string> LevelOne = {
    "............................................................",
    "............................................................",
    "............................................................",
    "............................................................",
    "............................................................",
    "............................................................",
    "............................................................",
    "............................CCC.............................",
    "...........................BBBBB............................",
    "...........CC.....C..............CC....CCC.....C............",
    "..........BBBB........................BBBBB.................",
    "........................##..................................",
    "..P...CCC...............##..........................C.C..E..",
    "#################...############....##########...###########",
    "#################...############....##########...###########",
};

const std::vector<std::string> LevelTwo = {
    "............................................................",
    "............................................................",
    "............................................................",
    "............................................................",
    "............................................................",
    "....................C.......................................",
    ".............................CC.............................",
    "...............C............BBBB............................",
    ".................CC....CC........................C..........",
    ".................##...BBBB.........CC.....CC................",
    "............C....##...............BBBB....##................",
    "...........###...##.......................##................",
    "..P..C.....###...##.......................##.............E..",
    "########...###...##.....................########...#########",
    "########...###...##.....................########...#########",
};

const int LastLevel = 2;

// ---------------------------------------------------------------------------
//  The player
// ---------------------------------------------------------------------------
struct Player : Entity {
    float runSpeed = 220;   // pixels per second
    float jumpSpeed = 600;  // how fast we shoot upwards when jumping
    float gravity = 1500;   // how fast we speed up when falling
    bool facingLeft = false;

    Player(Vec2 start) {
        position = start;
        size = {22, 28};
    }

    void update(float dt) override {
        // Left and right: -1, 0 or +1, times our running speed.
        float direction = getAxis("left", "right");
        velocity.x = direction * runSpeed;
        if (direction < 0) facingLeft = true;
        if (direction > 0) facingLeft = false;

        // Gravity pulls us down a little more every frame...
        velocity.y += gravity * dt;
        // ...but not forever, or we'd fall too fast to see.
        if (velocity.y > 900) velocity.y = 900;

        // Jump - but only when standing on something!
        bool jumpPressed = isActionPressed("jump") || isActionPressed("up");
        if (jumpPressed && isOnFloor()) {
            velocity.y = -jumpSpeed; // negative y = up
            scene().particles.burst(position + Vec2{0, 14}, Color::White, 8, 80, 0.3f, 3);
        }
        // Let go of jump while going up = a smaller jump. Feels great!
        bool jumpReleased = isActionReleased("jump") || isActionReleased("up");
        if (jumpReleased && velocity.y < 0) {
            velocity.y = velocity.y * 0.5f;
        }

        // moveAndSlide moves us by our velocity, but stops at solid tiles.
        // It also remembers if we landed, so isOnFloor() works next frame.
        moveAndSlide(dt);
    }

    void draw() override {
        drawRoundedRect(bounds(), 6, Color::fromHex(0xE74C3C));
        // The eyes look the way we're facing.
        float look = 4;
        if (facingLeft) look = -4;
        drawCircle(position + Vec2{look - 4, -4}, 3.5f, Color::White);
        drawCircle(position + Vec2{look + 4, -4}, 3.5f, Color::White);
        drawCircle(position + Vec2{look * 1.3f - 4, -4}, 1.5f, Color::Black);
        drawCircle(position + Vec2{look * 1.3f + 4, -4}, 1.5f, Color::Black);
    }
};

// ---------------------------------------------------------------------------
//  Coins and the exit flag
// ---------------------------------------------------------------------------
struct Coin : Entity {
    Coin(Vec2 where) {
        position = where;
        size = {20, 20};
    }

    void draw() override {
        // Bob up and down. Using position.x in the wave makes neighbours bob differently.
        Vec2 at = position + Vec2{0, std::sin(timeSinceStart() * 4 + position.x) * 3};
        drawCircle(at, 9, Color::Orange);
        drawCircle(at, 6, Color::Gold);
    }
};

struct Flag : Entity {
    Flag(Vec2 tileCenter) {
        // A tall hitbox, three tiles high, standing on the ground.
        size = {24, 96};
        position = tileCenter + Vec2{0, -32};
    }

    void draw() override {
        float bottom = bounds().bottom();
        float top = bounds().top();
        drawLine(Vec2{position.x, bottom}, Vec2{position.x, top}, Color::LightGray, 4);
        float wave = std::sin(timeSinceStart() * 6) * 4;
        drawTriangle(Vec2{position.x, top}, Vec2{position.x, top + 28},
                     Vec2{position.x + 40, top + 14 + wave}, Color::Lime);
    }
};

// ---------------------------------------------------------------------------
//  The "level complete" screen
// ---------------------------------------------------------------------------
struct WinScreen : Scene {
    int finishedLevel = 1;
    int coins = 0;
    int totalCoins = 0;

    WinScreen(int levelNumber, int coinsCollected, int coinsInLevel) {
        finishedLevel = levelNumber;
        coins = coinsCollected;
        totalCoins = coinsInLevel;
    }

    void start() override {
        background = Color::fromHex(0x2C3E50);
        particles.gravity = 300; // confetti falls down
        every(0.12f, [this] {
            Vec2 spot{randomFloat(50, 750), randomFloat(30, 150)};
            particles.burst(spot, randomColor(), 20, 200, 1.8f, 8);
        });
    }

    void update(float) override; // written at the bottom, because it needs PlatformLevel

    void drawUI() override {
        drawTextCentered("Level " + std::to_string(finishedLevel) + " complete!", Vec2{400, 170}, 50, Color::White);
        drawTextCentered("Coins: " + std::to_string(coins) + " / " + std::to_string(totalCoins),
                         Vec2{400, 235}, 30, Color::Gold);
        if (finishedLevel < LastLevel) {
            drawTextCentered("Press Enter for the next level", Vec2{400, 310}, 20, Color::LightGray);
        } else {
            drawTextCentered("You beat the whole game! Press Enter to play again", Vec2{400, 310}, 20,
                             Color::LightGray);
        }
    }
};

// ---------------------------------------------------------------------------
//  The level itself
// ---------------------------------------------------------------------------
struct PlatformLevel : Scene {
    int levelNumber = 1;
    TileMap map; // must be a member (not a local variable), because addSolid(map) remembers it
    Player* player = nullptr;
    Flag* flag = nullptr;
    int coinsCollected = 0;
    int totalCoins = 0;
    std::vector<Vec2> clouds;

    // changeScene<PlatformLevel>(2) passes the 2 in here.
    PlatformLevel(int number) {
        levelNumber = number;
    }

    void start() override {
        background = Color::fromHex(0x8ED6FF); // light blue sky

        // Pick which map to use.
        if (levelNumber == 1) {
            map = TileMap{32, LevelOne};
        } else {
            map = TileMap{32, LevelTwo};
            background = Color::fromHex(0xFFB37A); // level 2 happens at sunset
        }
        map.setSolid("#B"); // you can stand on ground and bricks
        map.setColor('#', Color::fromHex(0x8B5A2B));
        map.setColor('B', Color::fromHex(0xE0A030));

        // Turn the letters into real things. findTiles gives us the middle of every tile with that letter.
        for (Vec2 spot : map.findTiles('C')) {
            spawn<Coin>(spot);
        }
        totalCoins = count<Coin>();
        // [0] means "the first one" - there's only one P and one E in each map.
        player = &spawn<Player>(map.findTiles('P')[0]);
        flag = &spawn<Flag>(map.findTiles('E')[0]);

        // The letters were just markers, so turn them into empty sky.
        map.replaceTiles('C', '.');
        map.replaceTiles('P', '.');
        map.replaceTiles('E', '.');

        addSolid(map); // now moveAndSlide() knows about the ground

        // The camera never shows anything outside the level...
        camera.setLimits(map.bounds());
        // ...and starts on the player (smoothness 0 = jump straight there).
        camera.follow(player->position);

        for (int i = 0; i < 8; i++) {
            clouds.push_back(Vec2{i * 190.0f - 150, randomFloat(40, 170)});
        }
    }

    void update(float) override {
        // Collect coins.
        Coin* coin = player->findOverlapping<Coin>();
        if (coin != nullptr) {
            coinsCollected = coinsCollected + 1;
            particles.burst(coin->position, Color::Gold, 20, 160);
            coin->destroy();
        }

        // Reached the flag? Show the win screen, and tell it how we did.
        if (player->overlaps(*flag)) {
            changeScene<WinScreen>(levelNumber, coinsCollected, totalCoins);
        }

        // Fell into a pit? Try the level again.
        if (player->position.y > map.height() + 100) {
            restartScene();
        }

        // Invisible walls at the very start and end of the level.
        player->position.x = clamp(player->position.x, 11, map.width() - 11);

        // Follow the player smoothly. Bigger numbers = catches up faster.
        camera.follow(player->position, 8);
    }

    void draw() override {
        // Clouds move at HALF the camera's speed, so they look far away.
        // This trick is called "parallax".
        for (Vec2 cloud : clouds) {
            Vec2 at = cloud + Vec2{camera.position.x * 0.5f, 0};
            drawCircle(at, 22, Color::White.withAlpha(0.8f));
            drawCircle(at + Vec2{25, 6}, 18, Color::White.withAlpha(0.8f));
            drawCircle(at + Vec2{-25, 6}, 16, Color::White.withAlpha(0.8f));
        }

        map.draw();

        // Add some details by looking at every tile in the map:
        // grass on top of ground that has sky above it, and a border around each brick.
        for (int row = 0; row < map.rows(); row++) {
            for (int column = 0; column < map.columns(); column++) {
                Rect tile = map.tileRect(column, row);
                if (map.getTile(column, row) == '#' && map.getTile(column, row - 1) == '.') {
                    drawRect(tile.x, tile.y, tile.width, 8, Color::fromHex(0x4CAF50));
                }
                if (map.getTile(column, row) == 'B') {
                    drawRectOutline(tile, Color::fromHex(0x9A6418), 3);
                }
            }
        }
    }

    void drawUI() override {
        // A see-through dark box behind the text makes it easy to read over the bright sky.
        drawRoundedRect(Rect{8, 8, 190, 66}, 8, Color::Black.withAlpha(0.35f));
        drawText("Level " + std::to_string(levelNumber), 18, 14, 30, Color::White);
        drawText("Coins: " + std::to_string(coinsCollected) + " / " + std::to_string(totalCoins), 18, 47, 20,
                 Color::Gold);

        // measureText tells us how wide the text is, so we can line it up with the right edge.
        std::string help = "Arrows: run   Space: jump";
        float helpWidth = measureText(help, 20);
        drawRoundedRect(Rect{screenWidth() - helpWidth - 28, 8, helpWidth + 20, 34}, 8, Color::Black.withAlpha(0.35f));
        drawText(help, screenWidth() - helpWidth - 18, 15, 20, Color::White);
    }
};

// WinScreen's update, written down here because it needs to know about PlatformLevel.
void WinScreen::update(float) {
    if (isKeyPressed(Key::Enter) || isKeyPressed(Key::Space)) {
        if (finishedLevel < LastLevel) {
            changeScene<PlatformLevel>(finishedLevel + 1);
        } else {
            changeScene<PlatformLevel>(1); // back to the beginning
        }
    }
}

int main() {
    Game game("07 - Platformer", 800, 450);
    game.run<PlatformLevel>(1); // start at level 1
}
