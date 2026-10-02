// =============================================================================
//  Example 05 - Coin Collector
//
//  Grab as many coins as you can in 30 seconds!
//
//  What you'll learn:
//    - Entities: your own Player and Coin types that live in the scene
//    - spawn<Coin>(...) to add things, destroy() to remove them
//    - every(...) to do something again and again (spawning coins)
//    - findOverlapping<Coin>() to see what the player is touching
//    - a countdown with Timer
//    - a second scene (GameOver) and passing it the score with changeScene
//    - saving a high score with SaveFile
//    - particles.burst() for a satisfying sparkle
//
//  Controls:
//    Arrow keys / WASD   move
//    Space / Enter       play again (on the game over screen)
// =============================================================================
#include <sprout/sprout.hpp>

#include <cmath> // std::sin, std::ceil

using namespace sprout;

// ---------------------------------------------------------------------------
//  The Player - a happy green blob you move around.
// ---------------------------------------------------------------------------
struct Player : Entity {
    float speed = 320;

    Player(Vec2 startPosition) {
        position = startPosition; // remember: position is the MIDDLE
        size = {40, 40};          // the hitbox used for touching coins
    }

    void update(float dt) override {
        position += getVector("left", "right", "up", "down") * speed * dt;

        // Stay on the screen (half the size = from the middle to the edge).
        position.x = clamp(position.x, size.x / 2, screenWidth() - size.x / 2);
        position.y = clamp(position.y, size.y / 2, screenHeight() - size.y / 2);
    }

    void draw() override {
        drawRoundedRect(bounds(), 12, Color::Lime);
        // Two eyes, so it looks alive.
        drawCircle(position + Vec2{-8, -5}, 5, Color::White);
        drawCircle(position + Vec2{8, -5}, 5, Color::White);
        drawCircle(position + Vec2{-7, -4}, 2.5f, Color::Black);
        drawCircle(position + Vec2{9, -4}, 2.5f, Color::Black);
    }
};

// ---------------------------------------------------------------------------
//  A Coin - it just sits there and wobbles until someone grabs it.
// ---------------------------------------------------------------------------
struct Coin : Entity {
    float wobbleOffset = 0; // so the coins don't all wobble at exactly the same time

    Coin(Vec2 where) {
        position = where;
        size = {24, 24};
        wobbleOffset = randomFloat(0, 6);
    }

    void draw() override {
        // Bob up and down a little bit using a sine wave.
        Vec2 drawPosition = position + Vec2{0, std::sin(timeSinceStart() * 4 + wobbleOffset) * 3};
        drawCircle(drawPosition, 12, Color::Orange);
        drawCircle(drawPosition, 9, Color::Gold);
        drawCircle(drawPosition + Vec2{-3, -3}, 3, Color::Yellow.lighter(0.6f)); // a shiny spot
    }
};

// ---------------------------------------------------------------------------
//  The Game Over screen. It gets the final score from the level.
// ---------------------------------------------------------------------------
struct GameOver : Scene {
    int score = 0;
    int highScore = 0;
    bool newHighScore = false;

    // A SaveFile is a little file on your computer that remembers things
    // even after you close the game.
    SaveFile save{"coin_collector_save.json"};

    // changeScene<GameOver>(score) passes the score to this constructor.
    GameOver(int finalScore) {
        score = finalScore;
    }

    void start() override {
        background = Color::fromHex(0x1A1A2E);

        // get("highscore", 0) means: "give me the high score, or 0 if there isn't one yet".
        highScore = save.get("highscore", 0);
        if (score > highScore) {
            highScore = score;
            newHighScore = true;
            save.set("highscore", highScore); // written to the file straight away
        }
    }

    // This is only DECLARED here. It's written further down, after the
    // CoinLevel scene, because it needs to know about CoinLevel.
    void update(float) override;

    void drawUI() override {
        drawTextCentered("Time's up!", Vec2{400, 110}, 60, Color::White);
        drawTextCentered("You collected " + std::to_string(score) + " coins", Vec2{400, 190}, 30, Color::Gold);

        if (newHighScore) {
            // Make "NEW HIGH SCORE" flash in rainbow colors.
            drawTextCentered("NEW HIGH SCORE!", Vec2{400, 245}, 30, Color::fromHSV(timeSinceStart() * 200));
        } else {
            drawTextCentered("High score: " + std::to_string(highScore), Vec2{400, 245}, 30, Color::LightGray);
        }

        drawTextCentered("Press Space to play again", Vec2{400, 340}, 20, Color::Gray);
    }
};

// ---------------------------------------------------------------------------
//  The level, where the actual game happens.
// ---------------------------------------------------------------------------
struct CoinLevel : Scene {
    Player* player = nullptr;
    int score = 0;
    Timer roundTimer{30.0f}; // 30 seconds, starts counting right away

    void start() override {
        background = Color::fromHex(0x2B4162);

        // spawn<Player>(...) creates a Player and gives us a reference to it.
        // We keep a pointer (the address of the player) so we can use it later.
        player = &spawn<Player>(screenCenter());

        // A few coins to start with...
        for (int i = 0; i < 3; i++) {
            spawnCoin();
        }
        // ...and a new one every 0.8 seconds.
        every(0.8f, [this] { spawnCoin(); });
    }

    void spawnCoin() {
        // Not too many at once, or it gets too easy!
        if (count<Coin>() >= 10) {
            return;
        }
        // A random spot, but not too close to the edges or the score text.
        Vec2 spot = randomPointIn(Rect{40, 80, 720, 330});
        spawn<Coin>(spot);
    }

    void update(float) override { // we don't need dt here, so it has no name
        // Is the player touching a coin? findOverlapping gives us that coin,
        // or nullptr ("nothing") if it isn't touching any.
        Coin* coin = player->findOverlapping<Coin>();
        if (coin != nullptr) {
            score = score + 1;
            particles.burst(coin->position, Color::Gold, 25, 200); // sparkle!
            coin->destroy();
        }

        // When the timer runs out, go to the Game Over scene and give it the score.
        if (roundTimer.isDone()) {
            changeScene<GameOver>(score);
        }
    }

    void drawUI() override {
        drawText("Coins: " + std::to_string(score), 15, 12, 30, Color::Gold);

        // timeLeft() is a decimal like 12.37. std::ceil rounds it UP (to 13),
        // and static_cast<int> turns it into a whole number for showing.
        int secondsLeft = static_cast<int>(std::ceil(roundTimer.timeLeft()));
        Color timeColor = Color::White;
        if (secondsLeft <= 5) {
            timeColor = Color::Red; // hurry up!
        }
        std::string timeText = "Time: " + std::to_string(secondsLeft);
        drawText(timeText, screenWidth() - measureText(timeText, 30) - 15, 12, 30, timeColor);

        // A bar that shrinks as time runs out. progress() goes from 0 to 1.
        float barWidth = 300 * (1 - roundTimer.progress());
        drawRect(250, 22, 300, 12, Color::Black.withAlpha(0.3f));
        drawRect(250, 22, barWidth, 12, timeColor);
    }
};

// Here is GameOver's update(), written after CoinLevel so it can start a new one.
void GameOver::update(float) {
    if (isKeyPressed(Key::Space) || isKeyPressed(Key::Enter)) {
        changeScene<CoinLevel>();
    }
}

int main() {
    Game game("05 - Coin Collector", 800, 450);
    game.run<CoinLevel>();
}
