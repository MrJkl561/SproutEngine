// Moves a CharacterBody2D like a platformer hero: arrow keys / A D to run,
// Space / Up / W to jump. Attach it to a CharacterBody2D that has a
// CollisionShape2D child, and give the level some StaticBody2D floors.
#include "script.hpp"

using namespace s2d;

class PlayerController : public Script {
public:
    float speed = 260;
    float jumpSpeed = 520;
    float gravity = 1400;

    // Shown in the inspector, so each level (or player copy) can tune them.
    void exports(Exports& e) override {
        e.add("speed", speed);
        e.add("jump_speed", jumpSpeed);
        e.add("gravity", gravity, 5);
    }

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

        // Fell off the world? Start the level again.
        if (body->globalPosition().y > 1000) reloadScene();
    }
};

REGISTER_SCRIPT(PlayerController)
