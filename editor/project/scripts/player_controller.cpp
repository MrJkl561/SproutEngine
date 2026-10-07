// Moves a CharacterBody2D like a platformer hero: arrow keys / A D to run,
// Space / Up / W to jump. Attach it to a CharacterBody2D that has a
// CollisionShape2D child, and give the level some StaticBody2D floors. If it
// has an AnimatedSprite2D child with "idle", "run", "jump" and "fall"
// animations, it plays the right one.
#include "script.hpp"

using namespace s2d;

class PlayerController : public Script {
public:
    // Shown in the inspector, so each level (or player copy) can tune them.
    EXPORT(float, speed, 260);
    EXPORT(float, jumpSpeed, 520);
    EXPORT_STEP(float, gravity, 1400, 5);  // dragging it in the inspector goes 5 at a time

    void ready() override {
        if (!self<CharacterBody2D>()) print("PlayerController needs to be on a CharacterBody2D!");
    }

    void process(float delta) override {
        auto* body = self<CharacterBody2D>();
        if (!body) return;

        float direction = 0;
        if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) direction -= 1;
        if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) direction += 1;
        body->velocity.x = direction * speed;
        body->velocity.y += gravity * delta;

        bool jump = IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W);
        if (jump && body->isOnFloor()) body->velocity.y = -jumpSpeed;

        body->moveAndSlide();

        // Pick the animation that fits what the body is doing, and face the way it moves.
        if (auto* sprite = body->getChild<AnimatedSprite2D>()) {
            if (!body->isOnFloor()) sprite->play(body->velocity.y < 0 ? "jump" : "fall");
            else if (direction != 0) sprite->play("run");
            else sprite->play("idle");
            if (direction != 0) sprite->flipH = direction < 0;
        }

        // Fell off the world? Start the level again.
        if (body->globalPosition().y > 1000) reloadScene();
    }
};

REGISTER_SCRIPT(PlayerController)
