// =============================================================================
//  Example 06 - Space Shooter
//
//  Fly your ship, blast the asteroids, and don't get hit!
//  Big rocks break into smaller, faster rocks.
//
//  What you'll learn:
//    - three kinds of entities (Ship, Bullet, Asteroid) working together
//    - an entity spawning other entities:  scene().spawn<Bullet>(...)
//    - Cooldown, so the gun can't fire every single frame
//    - turning and flying with angles and Vec2::fromAngle()
//    - wrap() so things leaving one side of the screen come back on the other
//    - destroy() for bullets that fly off the screen
//    - "game juice": particles and camera.shake() make hits feel great
//    - lives, score, waves, and a Timer to stay safe after respawning
//
//  Controls:
//    Left / Right (or A / D)   turn
//    Up (or W)                 thrust
//    Space or X                shoot
//    R                         play again (after game over)
// =============================================================================
#include <sprout/sprout.hpp>

#include <vector>

using namespace sprout;

// ---------------------------------------------------------------------------
//  Bullet - flies in a straight line until it leaves the screen.
// ---------------------------------------------------------------------------
struct Bullet : Entity {
    Bullet(Vec2 start, Vec2 moveVelocity) {
        position = start;
        velocity = moveVelocity;
        size = {6, 6};
    }

    void update(float dt) override {
        position += velocity * dt;

        // Off the screen? Then it can never hit anything again, so destroy it.
        // Otherwise we'd have thousands of invisible bullets slowing the game down.
        Rect screen{0, 0, screenWidth(), screenHeight()};
        if (!screen.contains(position)) {
            destroy();
        }
    }

    void draw() override {
        drawCircle(position, 6, Color::Yellow.withAlpha(0.3f)); // a soft glow
        drawCircle(position, 3, Color::White);
    }
};

// ---------------------------------------------------------------------------
//  Asteroid - a spinning space rock. rockSize is 3 (big), 2 (medium) or 1 (small).
// ---------------------------------------------------------------------------
struct Asteroid : Entity {
    int rockSize = 3;
    float radius = 40;
    float rotation = 0;
    float spinSpeed = 0;
    int sides = 7;

    Asteroid(Vec2 where, int newRockSize) {
        position = where;
        rockSize = newRockSize;

        if (rockSize == 3) radius = 42;
        else if (rockSize == 2) radius = 26;
        else radius = 14;

        // The hitbox is a square a bit smaller than the rock, so the corners
        // of the square don't hit things that clearly missed the rock.
        size = {radius * 1.5f, radius * 1.5f};

        // Fly in a random direction. Smaller rocks are faster!
        float speed = randomFloat(40, 80) + (3 - rockSize) * 45;
        velocity = randomDirection() * speed;
        spinSpeed = randomFloat(-90, 90);
        sides = randomInt(6, 9);
    }

    void update(float dt) override {
        position += velocity * dt;
        rotation += spinSpeed * dt;

        // wrap(value, min, max): go past max and you come back at min (and the other way).
        // We add the radius so the rock is fully off screen before it jumps across.
        position.x = wrap(position.x, -radius, screenWidth() + radius);
        position.y = wrap(position.y, -radius, screenHeight() + radius);
    }

    void draw() override {
        Color rockColor = Color::fromHex(0x8D7B68);
        drawPolygon(position, sides, radius + 3, rockColor.darker(0.5f), rotation); // outline
        drawPolygon(position, sides, radius, rockColor, rotation);
        // A crater that turns with the rock.
        drawCircle(position + Vec2::fromAngle(rotation, radius * 0.4f), radius * 0.22f, rockColor.darker(0.3f));
    }
};

// ---------------------------------------------------------------------------
//  Ship - the player.
// ---------------------------------------------------------------------------
struct Ship : Entity {
    float angle = -90;         // which way the ship points, in degrees (-90 = up)
    Cooldown gun{0.18f};       // at most one shot every 0.18 seconds
    Timer safeTimer{2.0f};     // can't be hit until this is done

    Ship() {
        position = screenCenter();
        size = {20, 20};
    }

    bool isSafe() {
        return !safeTimer.isDone();
    }

    void respawn() {
        position = screenCenter();
        velocity = {0, 0};
        angle = -90;
        safeTimer.restart(); // 2 more seconds of safety
    }

    void update(float dt) override {
        // Turn left/right: getAxis gives -1, 0 or +1. 270 degrees per second.
        angle += getAxis("left", "right") * 270 * dt;

        // Thrust: speed up in the direction we're pointing.
        // Vec2::fromAngle(angle, length) makes an arrow pointing at that angle.
        if (isActionDown("up")) {
            velocity += Vec2::fromAngle(angle, 400 * dt);
            // Fire out the back! (angle + 180 is the opposite direction)
            Vec2 tail = position - Vec2::fromAngle(angle, 12);
            scene().particles.spray(tail, Color::Orange, angle + 180, 40, 2, 150, 0.3f, 4);
        }

        // Space has a tiny bit of friction in this game, so you slowly stop.
        velocity -= velocity * 0.6f * dt;
        // ...and a speed limit, so it stays controllable.
        if (velocity.length() > 350) {
            velocity = velocity.normalized() * 350;
        }
        position += velocity * dt;
        position.x = wrap(position.x, -15, screenWidth() + 15);
        position.y = wrap(position.y, -15, screenHeight() + 15);

        // Shooting. gun.use() is only true when the cooldown is ready.
        if (isActionDown("shoot") && gun.use()) {
            Vec2 nose = position + Vec2::fromAngle(angle, 22);
            scene().spawn<Bullet>(nose, Vec2::fromAngle(angle, 600));
        }
    }

    void draw() override {
        // While safe, blink: skip drawing every other tenth of a second.
        if (isSafe() && static_cast<int>(timeSinceStart() * 10) % 2 == 0) {
            return;
        }
        // Three corners of a triangle, turned to match the ship's angle.
        Vec2 nose = position + Vec2::fromAngle(angle, 22);
        Vec2 backLeft = position + Vec2::fromAngle(angle + 140, 16);
        Vec2 backRight = position + Vec2::fromAngle(angle - 140, 16);
        drawTriangle(nose, backLeft, backRight, Color::SkyBlue);
        drawCircle(position + Vec2::fromAngle(angle, 4), 4, Color::White); // the cockpit
    }
};

// ---------------------------------------------------------------------------
//  The level
// ---------------------------------------------------------------------------
struct SpaceLevel : Scene {
    Ship* ship = nullptr;
    int score = 0;
    int lives = 3;
    int wave = 0;
    bool gameOver = false;
    Timer waveMessageTimer{0}; // shows "Wave 2" for a little while
    std::vector<Vec2> stars;

    void start() override {
        background = Color::fromHex(0x0B0B1A);

        // Sprinkle some stars in the background. A std::vector is a list that can grow.
        for (int i = 0; i < 90; i++) {
            stars.push_back(randomPointIn(Rect{0, 0, screenWidth(), screenHeight()}));
        }

        ship = &spawn<Ship>();
        startNextWave();
    }

    void startNextWave() {
        wave = wave + 1;
        // Wave 1 has 3 big rocks, wave 2 has 4, and so on.
        for (int i = 0; i < wave + 2; i++) {
            // Start at the left or top edge, far from the ship in the middle.
            Vec2 spot{0, randomFloat(0, screenHeight())};
            if (chance(0.5f)) {
                spot = Vec2{randomFloat(0, screenWidth()), 0};
            }
            spawn<Asteroid>(spot, 3);
        }
        waveMessageTimer.restart(2.0f);
    }

    void breakAsteroid(Asteroid* rock) {
        rock->destroy();

        // Smaller rocks are harder to hit, so they're worth more.
        if (rock->rockSize == 3) score += 20;
        else if (rock->rockSize == 2) score += 50;
        else score += 100;

        // BOOM! Bigger rocks make bigger explosions.
        particles.burst(rock->position, Color::Orange, 10 * rock->rockSize, 80.0f * rock->rockSize + 100);
        particles.burst(rock->position, Color::fromHex(0x8D7B68), 8 * rock->rockSize, 120);
        camera.shake(2.0f * rock->rockSize, 0.2f);

        // Break into two smaller rocks.
        if (rock->rockSize > 1) {
            spawn<Asteroid>(rock->position, rock->rockSize - 1);
            spawn<Asteroid>(rock->position, rock->rockSize - 1);
        }
    }

    void shipWasHit() {
        lives = lives - 1;
        particles.burst(ship->position, Color::SkyBlue, 60, 300, 1.0f, 5);
        camera.shake(12, 0.5f);

        if (lives > 0) {
            ship->respawn();
        } else {
            ship->destroy();
            ship = nullptr; // the ship is gone, so forget about it
            gameOver = true;
        }
    }

    void update(float) override {
        if (gameOver) {
            if (isKeyPressed(Key::R)) {
                restartScene();
            }
            return;
        }

        // Did any bullet hit a rock?
        for (Bullet* bullet : findAll<Bullet>()) {
            Asteroid* rock = bullet->findOverlapping<Asteroid>();
            if (rock != nullptr) {
                bullet->destroy();
                breakAsteroid(rock);
            }
        }

        // Did a rock hit the ship? (Not while it's blinking and safe.)
        if (!ship->isSafe() && ship->findOverlapping<Asteroid>() != nullptr) {
            shipWasHit();
        }

        // All rocks gone? Here comes the next wave!
        if (!gameOver && count<Asteroid>() == 0) {
            startNextWave();
        }
    }

    void draw() override {
        int starNumber = 0;
        for (Vec2 star : stars) {
            float brightness = 0.2f + (starNumber % 5) * 0.15f; // some stars are brighter than others
            drawCircle(star, 1.5f, Color::White.withAlpha(brightness));
            starNumber = starNumber + 1;
        }
    }

    void drawUI() override {
        drawText("Score: " + std::to_string(score), 15, 12, 30, Color::White);

        // One little ship for each life left.
        for (int i = 0; i < lives; i++) {
            Vec2 spot{screenWidth() - 25.0f - i * 30, 28};
            drawTriangle(spot + Vec2{0, -12}, spot + Vec2{-9, 10}, spot + Vec2{9, 10}, Color::SkyBlue);
        }

        if (!waveMessageTimer.isDone()) {
            drawTextCentered("Wave " + std::to_string(wave), Vec2{400, 120}, 40, Color::Gold);
        }

        if (gameOver) {
            drawRect(0, 0, screenWidth(), screenHeight(), Color::Black.withAlpha(0.5f));
            drawTextCentered("GAME OVER", Vec2{400, 180}, 60, Color::Red);
            drawTextCentered("Score: " + std::to_string(score), Vec2{400, 240}, 30, Color::White);
            drawTextCentered("Press R to play again", Vec2{400, 290}, 20, Color::LightGray);
        }
    }
};

int main() {
    Game game("06 - Space Shooter", 800, 450);

    // A "shoot" action, so we can ask isActionDown("shoot") instead of
    // checking every key. ("left", "right" and "up" already exist.)
    bindAction("shoot", Key::Space);
    bindAction("shoot", Key::X);
    bindAction("shoot", GamepadButton::A);

    game.run<SpaceLevel>();
}
