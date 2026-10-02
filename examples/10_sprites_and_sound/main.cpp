// =============================================================================
//  Example 10 - Sprites and Sound
//
//  A little sprout who runs, jumps and collects stars - with real pictures,
//  animations, sound effects and music!
//
//  What you'll learn:
//    - Sprite: drawing a picture file instead of shapes
//    - sprite sheets: one picture with many animation frames in a grid
//    - addAnimation() and play() to switch between walk / idle / jump
//    - flipX to face left without needing a second set of pictures
//    - playSound() for effects (with a random pitch so they never get boring)
//    - playMusic() for background music that loops forever
//
//  The pictures and sounds are in the assets/ folder next to this file.
//  When you build, Sprout copies that folder next to the game automatically.
//
//  Controls:
//    Left / Right (or A / D)   walk
//    Space                     jump
//    M                         music on / off
// =============================================================================
#include <sprout/sprout.hpp>

#include <cmath> // std::sin
#include <vector>

using namespace sprout;

// ---------------------------------------------------------------------------
//  The player. It has a Sprite to look nice, and a hitbox (size) for physics.
// ---------------------------------------------------------------------------
struct Player : Entity {
    Sprite sprite{"assets/player.png"};
    float runSpeed = 220;
    float jumpSpeed = 600;
    float gravity = 1500;

    Player(Vec2 start) {
        position = start;
        size = {36, 60}; // the hitbox is a bit thinner than the picture

        // player.png is 4 frames wide and 2 frames high (each frame is 16x16 pixels).
        // Frames are numbered left-to-right, top-to-bottom:
        //    0 1 2 3     <- walking
        //    4 5 6 7     <- standing, jumping, blinking, falling
        sprite.setSpriteSheet(4, 2);
        sprite.addAnimation("walk", 0, 3, 10);              // frames 0 to 3, 10 frames per second
        sprite.addAnimation("idle", {4, 4, 4, 4, 4, 6}, 4); // stand still, and blink now and then
        sprite.addAnimation("jump", 5, 5, 1, false);        // just one frame; false = don't loop
        sprite.addAnimation("fall", 7, 7, 1, false);
        sprite.play("idle");

        // The picture is tiny (16 pixels), so draw it 4 times bigger.
        sprite.scale = {4, 4};
        // The anchor is the point of the picture that goes at the position we draw it.
        // {0.5, 1} is the bottom-middle: the sprout's feet.
        sprite.anchor = {0.5f, 1.0f};
    }

    void update(float dt) override {
        float direction = getAxis("left", "right");
        velocity.x = direction * runSpeed;
        velocity.y += gravity * dt;

        if (isOnFloor() && isActionPressed("jump")) {
            velocity.y = -jumpSpeed;
            // playSound(file, volume, pitch). A pitch a little above or below 1 makes the
            // jump sound slightly different every time, which sounds much more natural.
            playSound("assets/jump.wav", 0.6f, randomFloat(0.9f, 1.1f));
        }

        moveAndSlide(dt);
        position.x = clamp(position.x, size.x / 2, screenWidth() - size.x / 2);

        // Face the way we're walking. flipX mirrors the picture left-to-right.
        if (direction < 0) sprite.flipX = true;
        if (direction > 0) sprite.flipX = false;

        // Pick the animation that matches what we're doing right now.
        // (play() doesn't restart an animation that's already playing, so it's
        //  fine to call it every frame.)
        if (!isOnFloor()) {
            if (velocity.y < 0) {
                sprite.play("jump");
            } else {
                sprite.play("fall");
            }
        } else if (direction != 0) {
            sprite.play("walk");
        } else {
            sprite.play("idle");
        }

        sprite.update(dt); // moves the animation forward in time
    }

    void draw() override {
        // Draw the sprite with its anchor (the feet) at the bottom of our hitbox.
        sprite.draw(Vec2{position.x, bounds().bottom()});
    }
};

// ---------------------------------------------------------------------------
//  A star to collect. It spins and floats.
// ---------------------------------------------------------------------------
struct Star : Entity {
    Sprite sprite{"assets/star.png"};

    Star(Vec2 where) {
        position = where;
        size = {36, 36};
        sprite.scale = {3, 3};
    }

    void update(float dt) override {
        sprite.rotation += 90 * dt; // degrees per second
    }

    void draw() override {
        Vec2 bob{0, std::sin(timeSinceStart() * 3 + position.x) * 5};
        sprite.draw(position + bob);
    }
};

// ---------------------------------------------------------------------------
//  The scene
// ---------------------------------------------------------------------------
struct StarGarden : Scene {
    Player* player = nullptr;
    int starsCollected = 0;
    // The floor and three floating platforms.
    std::vector<Rect> platforms = {
        Rect{0, 400, 800, 50},
        Rect{90, 300, 170, 20},
        Rect{540, 300, 170, 20},
        Rect{315, 205, 170, 20},
    };

    void start() override {
        background = Color::fromHex(0x9BD4F5);
        for (Rect platform : platforms) {
            addSolid(platform);
        }
        player = &spawn<Player>(Vec2{400, 350});

        for (int i = 0; i < 3; i++) {
            spawnStar();
        }
        every(1.5f, [this] { spawnStar(); });

        // Background music! It loops forever, and calling playMusic again with the
        // same song won't restart it.
        // If the computer has no sound card (or a file is missing), playSound and
        // playMusic quietly do nothing - the game still runs, just silently.
        playMusic("assets/music.wav", 0.4f);
    }

    void spawnStar() {
        if (count<Star>() >= 5) return;
        // Somewhere above a random platform, low enough to reach with a jump.
        Rect platform = randomChoice(platforms);
        float x = randomFloat(platform.left() + 20, platform.right() - 20);
        Vec2 spot{x, platform.top() - randomFloat(30, 90)};

        // Not right on top of the player - that would be too easy!
        if (spot.distanceTo(player->position) < 100) return;
        // And not on top of another star. findClosest gives us the nearest one (or nullptr).
        Star* nearest = findClosest<Star>(spot);
        if (nearest != nullptr && spot.distanceTo(nearest->position) < 60) return;

        spawn<Star>(spot);
    }

    void update(float) override {
        Star* star = player->findOverlapping<Star>();
        if (star != nullptr) {
            starsCollected = starsCollected + 1;
            playSound("assets/pickup.wav", 0.7f, randomFloat(0.95f, 1.15f));
            particles.burst(star->position, Color::Gold, 25, 200);
            star->destroy();
        }

        if (isKeyPressed(Key::M)) {
            if (isMusicPlaying()) {
                pauseMusic();
            } else {
                resumeMusic();
            }
        }
    }

    void draw() override {
        // Rolling hills far away...
        drawCircle(Vec2{120, 470}, 190, Color::fromHex(0x8CCF7E));
        drawCircle(Vec2{460, 500}, 230, Color::fromHex(0x7BC26D));
        drawCircle(Vec2{760, 480}, 180, Color::fromHex(0x8CCF7E));
        // ...and the platforms: brown dirt with green grass on top.
        for (Rect platform : platforms) {
            drawRect(platform, Color::fromHex(0x8B5A2B));
            drawRect(platform.x, platform.y, platform.width, 8, Color::fromHex(0x4CAF50));
        }
    }

    void drawUI() override {
        drawText("Stars: " + std::to_string(starsCollected), 15, 12, 30, Color::fromHex(0x1B2A49));
        drawText("Arrows: walk   Space: jump   M: music", 15, 48, 15, Color::fromHex(0x1B2A49));

        // Show the whole sprite sheet in the corner, with a box around the
        // frame the player is showing right now. Walk and jump to see it change!
        Texture sheet = player->sprite.texture();
        Vec2 corner{652, 30};
        drawText("player.png", corner.x, corner.y - 18, 15, Color::fromHex(0x1B2A49));
        drawRect(corner.x, corner.y, 128, 64, Color::White.withAlpha(0.4f));
        drawTextureCentered(sheet, corner + Vec2{64, 32}, 2); // the 64x32 picture, twice as big
        int frame = player->sprite.frame();
        Rect frameBox{corner.x + (frame % 4) * 32, corner.y + (frame / 4) * 32, 32, 32};
        drawRectOutline(frameBox, Color::Red, 2);
        drawText("frame " + std::to_string(frame), corner.x, corner.y + 68, 15, Color::fromHex(0x1B2A49));
    }
};

int main() {
    Game game("10 - Sprites and Sound", 800, 450);
    game.run<StarGarden>();
}
