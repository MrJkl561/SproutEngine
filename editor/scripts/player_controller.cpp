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

        // Fell off the world? Start again at the top.
        if (body->position.y > 2000) {
            body->position = Vector2{100, 100};
            body->velocity = Vector2{0, 0};
        }
    }
};

REGISTER_SCRIPT(PlayerController)
